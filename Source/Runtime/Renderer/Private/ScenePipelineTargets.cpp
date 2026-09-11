#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
FGBufferLayout FGBufferLayout::HighPrecision()
{
	FGBufferLayout Result;
	Result.Formats.fill(EMaterialColorFormat::Rgba16Float);
	return Result;
}

void FGBufferLayout::Validate(const FRHICapabilities& InCapabilities) const
{
	if (InCapabilities.MaxColorTargets < Formats.size() || !InCapabilities.bSampledDepthTargets)
	{
		throw std::invalid_argument("Deferred requires four color targets and sampled D32 depth");
	}
	for (const auto Format : Formats)
	{
		if (!InCapabilities.SampledColorTargets.at(static_cast<std::size_t>(GetRenderColorFormat(Format))))
		{
			throw std::invalid_argument("GBuffer format lacks sampled render-target support");
		}
	}
	if (Formats[1] == EMaterialColorFormat::Rgba8Unorm || Formats[3] == EMaterialColorFormat::Rgba8Unorm)
	{
		throw std::invalid_argument("GBuffer normals and HDR emissive require floating-point storage");
	}
}

std::uint32_t FGBufferLayout::BytesPerPixel() const
{
	std::uint32_t Result{};
	for (const auto Format : Formats)
	{
		switch (Format)
		{
			case EMaterialColorFormat::Rgba8Unorm:
				Result += 4;
				break;
			case EMaterialColorFormat::Rgba16Float:
				Result += 8;
				break;
			case EMaterialColorFormat::Rgba32Float:
				Result += 16;
				break;
			default:
				throw std::invalid_argument("Unknown GBuffer format");
		}
	}
	return Result;
}

FSceneRenderPipeline::FSceneRenderPipeline(FRenderSession& InSession, FRHICapabilities InCapabilities,
                                           FScenePipelineSettings InSettings)
    : Session(InSession), Capabilities(std::move(InCapabilities)), Settings(std::move(InSettings))
{
	Configure(Settings);
}

void FSceneRenderPipeline::Configure(FScenePipelineSettings InSettings)
{
	if (InSettings.DebugMode > 6 ||
	    (InSettings.Pipeline != ESceneRenderPipeline::Deferred &&
	     InSettings.Pipeline != ESceneRenderPipeline::Forward) ||
	    !std::isfinite(InSettings.Exposure) || InSettings.Exposure <= 0 ||
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
	Settings = std::move(InSettings);
}

void FSceneRenderPipeline::Resize(std::uint32_t InWidth, std::uint32_t InHeight)
{
	if (InWidth == Width && InHeight == Height)
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
	auto NewDepth = std::make_shared<const FMaterialTextureSource>(FMaterialDepthTexture{InWidth, InHeight, 1});
	decltype(GBuffer) NewGBuffer;
	if (Settings.Pipeline == ESceneRenderPipeline::Deferred)
	{
		for (std::size_t Index = 0; Index < NewGBuffer.size(); ++Index)
		{
			NewGBuffer[Index] = std::make_shared<const FMaterialTextureSource>(
			    FMaterialColorTexture{InWidth, InHeight, Settings.GBuffer.Formats[Index]});
		}
	}
	Lifetime = std::move(NewLifetime);
	SceneColor = std::move(NewColor);
	SceneDepth = std::move(NewDepth);
	GBuffer = std::move(NewGBuffer);
	Width = InWidth;
	Height = InHeight;
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
	return {
	    {ERenderTargetKind::Texture, SceneDepth, Lifetime, false}, ERHIDepthFormat::D32, FAttachmentActions{InLoad}};
}

std::uint64_t FSceneRenderPipeline::TargetBytes() const
{
	return std::uint64_t(Width) * Height *
	       (12 + (Settings.Pipeline == ESceneRenderPipeline::Deferred ? Settings.GBuffer.BytesPerPixel() : 0));
}

const FCascadedShadowMap& FSceneRenderPipeline::Shadows() const
{
	return ShadowMaps;
}
} // namespace Hyperion
