#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
FSceneRenderPipeline::FSceneRenderPipeline(FRenderSession& InSession, FRHICapabilities InCapabilities,
                                           FScenePipelineSettings InSettings, FRenderFeatureList InFeatures)
    : Session(InSession), Capabilities(std::move(InCapabilities)), Settings(std::move(InSettings)),
      Features(std::move(InFeatures))
{
	Configure(Settings);
}

void FSceneRenderPipeline::Configure(FScenePipelineSettings InSettings)
{
	InSettings.ContactShadows.Validate();
	(void)DescribeSceneRenderPipeline(InSettings.Pipeline);
	(void)ParseGBufferVisualizer(InSettings.DebugMode);
	if (!std::isfinite(InSettings.Exposure) || InSettings.Exposure <= 0 ||
	    !Capabilities.SampledColorTargets.at(static_cast<std::size_t>(ERHIColorFormat::Rgba16Float)))
	{
		throw std::invalid_argument("Scene pipeline requires positive exposure and sampled RGBA16F targets");
	}
	if (InSettings.Pipeline == ESceneRenderPipeline::Deferred)
	{
		InSettings.GBuffer.Validate(Capabilities);
	}
	if (Settings.Pipeline != InSettings.Pipeline || Settings.GBuffer != InSettings.GBuffer)
	{
		Session.ResetViewHistory();
		Width = 0;
		Height = 0;
	}
	DefaultContactShadows = InSettings.ContactShadows;
	Settings = std::move(InSettings);
}

FScenePipelineSettings FSceneRenderPipeline::Configuration() const
{
	return Settings;
}

void FSceneRenderPipeline::Resize(std::uint32_t InWidth, std::uint32_t InHeight, EDepthConvention InConvention)
{
	if (InWidth == Width && InHeight == Height && InConvention == DepthConvention)
	{
		return;
	}
	if (!InWidth || !InHeight || InWidth > Capabilities.MaxTextureDimension ||
	    InHeight > Capabilities.MaxTextureDimension)
	{
		throw std::invalid_argument("Scene target dimensions exceed device limits");
	}
	// Prepare an entire immutable generation before replacing the previous generation.
	auto NewLifetime = Session.GetResources().CreateScopeLifetime();
	auto NewColor = std::make_shared<const FMaterialTextureSource>(
	    FMaterialColorTexture{InWidth, InHeight, EMaterialColorFormat::Rgba16Float});
	auto NewDepth = std::make_shared<const FMaterialTextureSource>(
	    FMaterialDepthTexture{InWidth, InHeight, GetDepthClearValue(InConvention)});
	decltype(GBuffer) NewGBuffer;
	if (Settings.Pipeline == ESceneRenderPipeline::Deferred)
	{
		for (const auto& Attachment : GBufferAttachments())
		{
			GBufferAttachment(NewGBuffer, Attachment.Role) = std::make_shared<const FMaterialTextureSource>(
			    FMaterialColorTexture{InWidth, InHeight, GBufferAttachment(Settings.GBuffer.Formats, Attachment.Role)});
		}
	}
	Lifetime = std::move(NewLifetime);
	SceneColor = std::move(NewColor);
	SceneDepth = std::move(NewDepth);
	GBuffer = std::move(NewGBuffer);
	Width = InWidth;
	Height = InHeight;
	DepthConvention = InConvention;
}

void FSceneRenderPipeline::ClearTargets(FRenderGraph& InGraph, FVec4 InClear) const
{
	FRenderSceneSnapshot Snapshot;
	Snapshot.Targets = ColorTargets("Scene/Clear", EAttachmentLoad::Clear, InClear);
	Snapshot.Targets.DepthStencil = DepthTarget(EAttachmentLoad::Clear);
	InGraph.Add(Session.GetResources().GetPreparation().DeclarePass(InGraph, Snapshot));
	Snapshot.Targets = {};
	Snapshot.Targets.Name = "Scene/GBufferClear";
	for (const auto& Texture : GBuffer)
	{
		if (Texture)
		{
			Snapshot.Targets.Colors.push_back(
			    {{ERenderTargetKind::Texture, Texture, Lifetime, false}, {EAttachmentLoad::Clear}});
		}
	}
	if (!Snapshot.Targets.Colors.empty())
	{
		InGraph.Add(Session.GetResources().GetPreparation().DeclarePass(InGraph, Snapshot));
	}
}

FRenderPassTargets FSceneRenderPipeline::ColorTargets(std::string InName, EAttachmentLoad InLoad, FVec4 InClear) const
{
	FRenderPassTargets Result;
	Result.Name = std::move(InName);
	Result.Color = FRenderColorTarget{{ERenderTargetKind::Texture, SceneColor, Lifetime, false}, {InLoad}, InClear};
	return Result;
}

FRenderDepthTarget FSceneRenderPipeline::DepthTarget(EAttachmentLoad InLoad) const
{
	return {{ERenderTargetKind::Texture, SceneDepth, Lifetime, false},
	        ERHIDepthFormat::D32,
	        FAttachmentActions{InLoad},
	        {},
	        GetDepthClearValue(DepthConvention)};
}

std::uint64_t FSceneRenderPipeline::TargetBytes() const
{
	std::uint64_t FeatureBytes{};
	for (const auto& Feature : Features)
	{
		FeatureBytes += Feature->ResourceBytes();
	}
	return std::uint64_t(Width) * Height *
	           (12 + (Settings.Pipeline == ESceneRenderPipeline::Deferred ? Settings.GBuffer.BytesPerPixel() : 0)) +
	       FeatureBytes;
}

const FCascadedShadowMap& FSceneRenderPipeline::Shadows() const
{
	return ShadowMaps;
}
} // namespace Hyperion
