#include "DirectionalLighting.h"
#include "EnvironmentParameters.h"
#include "Hyperion/Materials/Lighting/SceneLightingParameters.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Renderer/ShaderParameters/DeferredLightingParameters.h"
#include "Hyperion/Renderer/ShaderParameters/OutputParameters.h"
#include <bit>

namespace Hyperion
{
namespace
{
FViewport Viewport(const FRenderView& InView)
{
	return InView.Viewport.value_or(FViewport{0, 0, float(InView.Width), float(InView.Height)});
}

std::shared_ptr<const FMaterialDefinition> LightingMaterial(std::string InName, std::string InShader)
{
	// Fullscreen passes receive already resolved values as manual parameters, without scene providers.
	auto Description = MakeFullscreenMaterial(std::move(InName), std::move(InShader))->GetDescription();
	Description.ShaderContracts = {GetDeferredLightingShaderContracts()};
	return std::make_shared<const FMaterialDefinition>(std::move(Description));
}

std::shared_ptr<const FMaterialDefinition> GBufferDebugMaterial()
{
	auto Description =
	    MakeFullscreenMaterial("GBuffer debug", "Deferred/Debug.hlsl", true, {GetDeferredLightingShaderContracts()})
	        ->GetDescription();
	Description.Passes.front().Pixel.Defines = MakeGBufferVisualizerShaderDefines();
	return std::make_shared<const FMaterialDefinition>(std::move(Description));
}

void SetLightingParameters(FFullscreenPassDesc& InPass, FRenderSession& InSession, const FMaterialFrameContext& InFrame,
                           const FRenderView& InMain, bool bInNoDirectional)
{
	FDeferredLightV1Parameters Parameters;
	Parameters.InverseViewProjection = Inverse(InMain.ViewProjection);
	const auto View = InPass.Viewport;
	Parameters.Viewport = {View.X, View.Y, View.Width, View.Height};
	Parameters.DepthRange = {View.MinDepth, 1.f / (View.MaxDepth - View.MinDepth)};
	Parameters.Eye = InMain.Eye;
	const auto Directionals = InSession.ResolveFrameSemantic(InFrame, DirectionalLightsSemantic);
	InPass.Parameters.push_back(
	    {ESceneLightingSemantic::DirectionalLights, Directionals ? *Directionals : DirectionalLightBuffer()});
	for (const auto& Default : EnvironmentParameters())
	{
		const auto Value = InSession.ResolveFrameSemantic(InFrame, Default.GetSemantic());
		InPass.Parameters.push_back({Default.GetSemantic(), Value ? *Value : Default.Value});
	}
	const auto SceneVector = [&](FMaterialSemanticId InSemantic)
	{
		const auto Value = InSession.ResolveFrameSemantic(InFrame, InSemantic);
		if (!Value || Value->Type != FMaterialParameterType::Numeric(EMaterialScalar::Float, 3))
		{
			throw std::invalid_argument("Deferred lighting requires frame-resolved scene light semantics");
		}
		return FVec3{std::bit_cast<float>(Value->Words[0]), std::bit_cast<float>(Value->Words[1]),
		             std::bit_cast<float>(Value->Words[2])};
	};
	Parameters.LightDirection = bInNoDirectional ? FVec3{} : SceneVector(EHyperionSceneV1Field::MainLightDirection);
	Parameters.LightColor = bInNoDirectional ? FVec3{} : SceneVector(EHyperionSceneV1Field::MainLightColor);
	Parameters.Ambient = SceneVector(EHyperionSceneV1Field::AmbientColor);
	AppendShaderParameters(InPass.Parameters, Parameters);
}

void BindGBuffer(FFullscreenPassDesc& InPass,
                 const std::array<std::shared_ptr<const FMaterialTextureSource>, GBufferAttachmentCount>& InTextures,
                 const std::shared_ptr<const void>& InLifetime)
{
	for (const auto& Attachment : GBufferAttachments())
	{
		const auto& Texture = GBufferAttachment(InTextures, Attachment.Role);
		InPass.Targets.Reads.push_back({ERenderTargetKind::Texture, Texture, InLifetime, false});
		InPass.Parameters.push_back({Attachment.Semantic, FMaterialValue::FromTexture(Texture)});
	}
}
} // namespace

FFullscreenPassDesc FSceneRenderPipeline::Lighting(const FRenderView& InMain, const FMaterialFrameContext& InFrame,
                                                   FVec4 InClear) const
{
	static const auto Material = LightingMaterial("Deferred lighting", "Deferred/Lighting.hlsl");
	static const auto Clustered = LightingMaterial("Deferred clustered lighting", "Deferred/Clustered.hlsl");
	static const auto ClusterOnly = LightingMaterial("Deferred cluster only", "Deferred/ClusteredOnly.hlsl");
	static const auto Contact = LightingMaterial("Deferred contact lighting", "Deferred/LightingContact.hlsl");
	static const auto ClusterContact =
	    LightingMaterial("Deferred clustered contact lighting", "Deferred/ClusteredContact.hlsl");
	const bool bClustered = Settings.bClusteredLighting && LastStatistics.LocalLights.bActive;
	const auto Direct = Session.ResolveFrameSemantic(InFrame, EHyperionSceneV1Field::MainLightColor);
	if (!Direct || Direct->Type != FMaterialParameterType::Numeric(EMaterialScalar::Float, 3))
	{
		throw std::invalid_argument("Deferred lighting requires scene directional radiance");
	}
	const bool bNoDirectional = bClustered && Direct->Words == FMaterialValue::Float(FVec3{}).Words;
	FFullscreenPassDesc Result;
	Result.Material = bClustered ? (bNoDirectional ? ClusterOnly : Clustered) : Material;
	if (FeatureResources.DirectionalVisibility.Texture && !bNoDirectional)
	{
		Result.Material = bClustered ? ClusterContact : Contact;
		Result.Parameters.push_back({EDeferredLightingSemantic::ContactVisibility,
		                             FMaterialValue::FromTexture(FeatureResources.DirectionalVisibility.Texture)});
		FMaterialSampler Sampler;
		Sampler.U = Sampler.V = Sampler.W = EMaterialAddressMode::Clamp;
		Sampler.bMinLinear = false;
		Sampler.bMagLinear = false;
		Sampler.bMipLinear = false;
		Result.Parameters.push_back({EDeferredLightingSemantic::ContactSampler, FMaterialValue::FromSampler(Sampler)});
	}
	Result.DepthConvention = InMain.DepthConvention;
	Result.Lifetime = Lifetime;
	// Manual fullscreen bindings must retire with their immutable scene inputs, including directional buffers.
	Result.ResourceLifetime = InFrame.Inputs.Scopes[static_cast<std::size_t>(EMaterialScope::Scene)].Lifetime;
	Result.Statistics = FullscreenStatistics;
	Result.Viewport = Viewport(InMain);
	Result.Targets =
	    ColorTargets("Deferred/Lighting", InMain.Viewport ? EAttachmentLoad::Load : EAttachmentLoad::Clear, InClear);
	Result.Targets.Timing.Category = ERenderPassTimingCategory::Lighting;
	if (FeatureResources.DirectionalVisibility.Texture && !bNoDirectional)
	{
		Result.Targets.Reads.push_back(FeatureResources.DirectionalVisibility);
	}
	if (bClustered)
	{
		Result.Targets.Name = bNoDirectional ? "Deferred/ClusterLighting" : "Deferred/LightingClustered";
		Result.ParameterLifetime = ClusterLifetime;
		Result.Parameters.insert(Result.Parameters.end(), ClusterParameters.begin(), ClusterParameters.end());
	}
	auto ShadowView = InMain;
	if (!bNoDirectional)
	{
		ShadowMaps.Bind(ShadowView, Result.Targets, ShadowLifetime);
	}
	for (const auto& Parameter : ShadowView.Parameters)
	{
		if (!bNoDirectional && GetEngineSemanticPolicy(Parameter.GetSemantic()).Group == EEngineSemanticGroup::Shadow)
		{
			Result.Parameters.push_back(Parameter);
			if (Parameter.Value.Texture && !LastStatistics.bShadows)
			{
				Result.Targets.Reads.push_back({ERenderTargetKind::Texture, Parameter.Value.Texture, Lifetime, true});
			}
		}
	}
	BindGBuffer(Result, GBuffer, Lifetime);
	Result.Targets.Reads.push_back({ERenderTargetKind::Texture, SceneDepth, Lifetime, false});
	Result.Parameters.push_back({EDeferredLightingSemantic::SceneDepth, FMaterialValue::FromTexture(SceneDepth)});
	SetLightingParameters(Result, Session, InFrame, InMain, bNoDirectional);
	return Result;
}

FFullscreenPassDesc FSceneRenderPipeline::Debug(const FRenderView& InMain) const
{
	static const auto Material = GBufferDebugMaterial();
	FFullscreenPassDesc Result;
	Result.Material = Material;
	Result.DepthConvention = InMain.DepthConvention;
	Result.Lifetime = Lifetime;
	Result.Statistics = FullscreenStatistics;
	Result.Viewport = Viewport(InMain);
	Result.Targets = OutputTargets();
	Result.Targets.Name = "Deferred/GBuffer debug";
	BindGBuffer(Result, GBuffer, Lifetime);
	Result.Targets.Reads.push_back({ERenderTargetKind::Texture, SceneDepth, Lifetime, false});
	Result.Parameters.push_back({EDeferredLightingSemantic::SceneDepth, FMaterialValue::FromTexture(SceneDepth)});
	AppendShaderParameters(Result.Parameters, FGBufferDebugV1Parameters{ToVisualizerShaderCode(Settings.DebugMode)});
	return Result;
}

FFullscreenPassDesc FSceneRenderPipeline::Tonemap(const FRenderView& InMain) const
{
	static const auto Material =
	    MakeFullscreenMaterial("HDR tonemap", "Common/Tonemap.hlsl", true, {GetOutputShaderContracts()});
	FFullscreenPassDesc Result;
	Result.Material = Material;
	Result.DepthConvention = InMain.DepthConvention;
	Result.Lifetime = Lifetime;
	Result.Statistics = FullscreenStatistics;
	Result.bFullTargetViewport = true;
	// Initialize the entire presentation surface even for a sub-viewport.
	Result.Viewport = {0, 0, float(InMain.Width), float(InMain.Height)};
	Result.Targets = OutputTargets(FVec4{});
	Result.Targets.Name = "Output/Tonemap";
	Result.Targets.Timing.Category = ERenderPassTimingCategory::Tonemap;
	Result.Targets.Reads = {{ERenderTargetKind::Texture, SceneColor, Lifetime, false}};
	Result.Parameters = {{EOutputSemantic::SceneColor, FMaterialValue::FromTexture(SceneColor)}};
	AppendShaderParameters(Result.Parameters, FOutputV1Parameters{Settings.Exposure});
	return Result;
}
} // namespace Hyperion
