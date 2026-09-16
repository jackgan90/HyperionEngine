#include "EnvironmentParameters.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"

namespace Hyperion
{
namespace
{
FViewport Viewport(const FRenderView& InView)
{
	return InView.Viewport.value_or(FViewport{0, 0, float(InView.Width), float(InView.Height)});
}

void SetLightingParameters(FFullscreenPassDesc& InPass, FRenderSession& InSession, const FMaterialFrameContext& InFrame,
                           const FRenderView& InMain, bool bInNoDirectional)
{
	const auto Set = [&](std::string InName, FMaterialValue InValue)
	{
		InPass.Parameters.push_back({"Pixel:DeferredLightV1." + InName, std::move(InValue)});
	};
	Set("InverseViewProjection", FMaterialValue::Matrix(Inverse(InMain.ViewProjection)));
	const auto View = InPass.Viewport;
	Set("Viewport", FMaterialValue::Float(FVec4{View.X, View.Y, View.Width, View.Height}));
	Set("DepthRange", FMaterialValue::Float(FVec2{View.MinDepth, 1.f / (View.MaxDepth - View.MinDepth)}));
	Set("Eye", FMaterialValue::Float(InMain.Eye));
	for (const auto& Default : EnvironmentParameters())
	{
		const auto Value = InSession.ResolveFrameSemantic(InFrame, Default.Name);
		const auto Name = Default.Name.substr(std::string("Engine.Scene.").size());
		InPass.Parameters.push_back(
		    {"Pixel:" + (Default.Value.Type.Kind == EMaterialValueKind::Numeric ? "EnvironmentV1." + Name : Name),
		     Value ? *Value : Default.Value});
	}
	for (const auto& Mapping : {std::pair{"LightDirection", "Engine.Scene.MainDirectionalLightDirection"},
	                            std::pair{"LightColor", "Engine.Scene.MainDirectionalLightColor"},
	                            std::pair{"Ambient", "Engine.Scene.AmbientColor"}})
	{
		if (bInNoDirectional && std::string_view(Mapping.first) != "Ambient")
		{
			continue;
		}
		const auto Value = InSession.ResolveFrameSemantic(InFrame, Mapping.second);
		if (!Value || Value->Type != FMaterialParameterType::Numeric(EMaterialScalar::Float, 3))
		{
			throw std::invalid_argument("Deferred lighting requires frame-resolved scene light semantics");
		}
		Set(Mapping.first, *Value);
	}
}
} // namespace

FFullscreenPassDesc FSceneRenderPipeline::Lighting(const FRenderView& InMain, const FMaterialFrameContext& InFrame,
                                                   FVec4 InClear) const
{
	static const auto Material = MakeFullscreenMaterial("Deferred lighting", "Deferred/Lighting.hlsl");
	static const auto Clustered = MakeFullscreenMaterial("Deferred clustered lighting", "Deferred/Clustered.hlsl");
	static const auto ClusterOnly = MakeFullscreenMaterial("Deferred cluster only", "Deferred/ClusteredOnly.hlsl");
	static const auto Contact = MakeFullscreenMaterial("Deferred contact lighting", "Deferred/LightingContact.hlsl");
	static const auto ClusterContact =
	    MakeFullscreenMaterial("Deferred clustered contact lighting", "Deferred/ClusteredContact.hlsl");
	const bool bClustered = Settings.bClusteredLighting && LastStatistics.LocalLights.bActive;
	const auto Direct = Session.ResolveFrameSemantic(InFrame, "Engine.Scene.MainDirectionalLightColor");
	if (!Direct || Direct->Type != FMaterialParameterType::Numeric(EMaterialScalar::Float, 3))
	{
		throw std::invalid_argument("Deferred lighting requires scene directional radiance");
	}
	const bool bNoDirectional = bClustered && Direct->Words == FMaterialValue::Float(FVec3{}).Words;
	FFullscreenPassDesc Result;
	Result.Material = bClustered ? (bNoDirectional ? ClusterOnly : Clustered) : Material;
	if (LastStatistics.bContactShadows && !bNoDirectional)
	{
		Result.Material = bClustered ? ClusterContact : Contact;
		Result.Parameters.push_back({"Pixel:ContactVisibility", FMaterialValue::FromTexture(ContactMask)});
		FMaterialSampler Sampler;
		Sampler.U = Sampler.V = Sampler.W = EMaterialAddressMode::Clamp;
		Sampler.bMinLinear = false;
		Sampler.bMagLinear = false;
		Sampler.bMipLinear = false;
		Result.Parameters.push_back({"Pixel:ContactSampler", FMaterialValue::FromSampler(Sampler)});
	}
	Result.DepthConvention = InMain.DepthConvention;
	Result.Lifetime = Lifetime;
	if (const auto Metadata = InFrame.GetSceneMetadata(); Metadata && Metadata->Settings.EnvironmentLight)
	{
		const auto Found = Metadata->EnvironmentLights.find(*Metadata->Settings.EnvironmentLight);
		if (Found != Metadata->EnvironmentLights.end())
		{
			Result.ResourceLifetime = Found->second.Light.Data;
		}
	}
	Result.Statistics = FullscreenStatistics;
	Result.Viewport = Viewport(InMain);
	Result.Targets =
	    ColorTargets("Deferred/Lighting", InMain.Viewport ? EAttachmentLoad::Load : EAttachmentLoad::Clear, InClear);
	if (LastStatistics.bContactShadows && !bNoDirectional)
	{
		Result.Targets.Reads.push_back({ERenderTargetKind::Texture, ContactMask, ContactLifetime, false});
	}
	if (bClustered)
	{
		Result.Targets.Name = bNoDirectional ? "Deferred/ClusterLighting" : "Deferred/LightingClustered";
		Result.ParameterLifetime = ClusterLifetime;
		for (const auto& Parameter : ClusterParameters)
		{
			const auto Name = Parameter.Name.substr(std::string("Engine.View.").size());
			Result.Parameters.push_back(
			    {"Pixel:" + (Parameter.Value.Type.Kind == EMaterialValueKind::Numeric ? "ClusterViewV1." + Name : Name),
			     Parameter.Value});
		}
	}
	auto ShadowView = InMain;
	if (!bNoDirectional)
	{
		ShadowMaps.Bind(ShadowView, Result.Targets, ShadowLifetime);
	}
	for (const auto& Parameter : ShadowView.Parameters)
	{
		if (!bNoDirectional && Parameter.Name.starts_with("Engine.View.Shadow"))
		{
			const auto Name = Parameter.Name.substr(std::string("Engine.View.").size());
			Result.Parameters.push_back(
			    {"Pixel:" + (Parameter.Value.Type.Kind == EMaterialValueKind::Numeric ? "ShadowViewV1." + Name : Name),
			     Parameter.Value});
			if (Parameter.Value.Texture && !LastStatistics.bShadows)
			{
				Result.Targets.Reads.push_back({ERenderTargetKind::Texture, Parameter.Value.Texture, Lifetime, true});
			}
		}
	}
	for (std::size_t Index = 0; Index < GBuffer.size(); ++Index)
	{
		Result.Targets.Reads.push_back({ERenderTargetKind::Texture, GBuffer[Index], Lifetime, false});
		Result.Parameters.push_back(
		    {"Pixel:GBuffer" + std::to_string(Index), FMaterialValue::FromTexture(GBuffer[Index])});
	}
	Result.Targets.Reads.push_back({ERenderTargetKind::Texture, SceneDepth, Lifetime, false});
	Result.Parameters.push_back({"Pixel:SceneDepth", FMaterialValue::FromTexture(SceneDepth)});
	SetLightingParameters(Result, Session, InFrame, InMain, bNoDirectional);
	return Result;
}

FFullscreenPassDesc FSceneRenderPipeline::Debug(const FRenderView& InMain) const
{
	static const auto Material = MakeFullscreenMaterial("GBuffer debug", "Deferred/Debug.hlsl", true);
	FFullscreenPassDesc Result;
	Result.Material = Material;
	Result.DepthConvention = InMain.DepthConvention;
	Result.Lifetime = Lifetime;
	Result.Statistics = FullscreenStatistics;
	Result.Viewport = Viewport(InMain);
	Result.Targets = OutputTargets();
	Result.Targets.Name = "Deferred/GBuffer debug";
	for (std::size_t Index = 0; Index < GBuffer.size(); ++Index)
	{
		Result.Targets.Reads.push_back({ERenderTargetKind::Texture, GBuffer[Index], Lifetime, false});
		Result.Parameters.push_back(
		    {"Pixel:GBuffer" + std::to_string(Index), FMaterialValue::FromTexture(GBuffer[Index])});
	}
	Result.Targets.Reads.push_back({ERenderTargetKind::Texture, SceneDepth, Lifetime, false});
	Result.Parameters.push_back({"Pixel:SceneDepth", FMaterialValue::FromTexture(SceneDepth)});
	Result.Parameters.push_back({"Pixel:GBufferDebugV1.Mode", FMaterialValue::Uint(Settings.DebugMode)});
	return Result;
}

FFullscreenPassDesc FSceneRenderPipeline::Tonemap(const FRenderView& InMain) const
{
	static const auto Material = MakeFullscreenMaterial("HDR tonemap", "Common/Tonemap.hlsl", true);
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
	Result.Targets.Reads = {{ERenderTargetKind::Texture, SceneColor, Lifetime, false}};
	Result.Parameters = {{"Pixel:SceneColor", FMaterialValue::FromTexture(SceneColor)},
	                     {"Pixel:OutputV1.Exposure", FMaterialValue::Float(Settings.Exposure)}};
	return Result;
}
} // namespace Hyperion
