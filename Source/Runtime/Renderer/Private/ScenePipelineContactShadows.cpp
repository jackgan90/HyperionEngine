#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include <algorithm>
#include <bit>

namespace Hyperion
{
namespace
{
class FContactShadowFeature final : public IRenderFeature
{
public:
	void BeginFrame(FRenderFeatureContext& InContext) override;
	void Build(ERenderFeatureStage InStage, FRenderFeatureContext& InContext) override;
	void EndFrame(FRenderFeatureContext& InContext) override;
	void Reset() override;
	std::uint64_t ResourceBytes() const override;

private:
	void AddShadows(FRenderFeatureContext& InContext);
	void AddDebug(FRenderFeatureContext& InContext);
	FHierarchicalDepthProducer HierarchicalDepth;
	FHierarchicalDepthProduct Depth;
	FRenderTargetSource Source;
	FRenderTargetSource Mask;
	bool bActive{};
};

void FContactShadowFeature::BeginFrame(FRenderFeatureContext& InContext)
{
	if (Source.Texture != InContext.Resources.Depth.Texture)
	{
		Mask = {};
	}
	Source = InContext.Resources.Depth;
	Depth = {};
	bActive = false;
	HierarchicalDepth.BeginFrame(InContext.Graph);
}

void FContactShadowFeature::Build(ERenderFeatureStage InStage, FRenderFeatureContext& InContext)
{
	if (InContext.Settings.Pipeline != ESceneRenderPipeline::Deferred)
	{
		return;
	}
	if (InStage == ERenderFeatureStage::BeforeLighting)
	{
		AddShadows(InContext);
	}
	else if (InStage == ERenderFeatureStage::AfterTonemap)
	{
		AddDebug(InContext);
	}
}

void FContactShadowFeature::AddShadows(FRenderFeatureContext& InContext)
{
	auto& Session = InContext.Session;
	const auto& Settings = InContext.Settings.ContactShadows;
	const auto Direct = Session.ResolveFrameSemantic(InContext.Frame, "Engine.Scene.MainDirectionalLightColor");
	const bool bDirectional = Direct && Direct->Words != FMaterialValue::Float(FVec3{}).Words;
	bActive =
	    Settings.bEnabled && bDirectional && (!InContext.Frame.GetSceneToken() || InContext.Frame.CastsSceneShadows());
	InContext.Statistics.bContactShadows = bActive;
	const FHierarchicalDepthRequest Request{Source, InContext.View};
	if (bActive)
	{
		Depth = HierarchicalDepth.Request(Session, InContext.Graph, Request, InContext.bDeferPreparation);
		if (!Mask.Texture)
		{
			Mask = {ERenderTargetKind::Texture,
			        std::make_shared<const FMaterialTextureSource>(FMaterialColorTexture{
			            InContext.View.Width, InContext.View.Height, EMaterialColorFormat::R8Unorm}),
			        Session.GetResources().CreateScopeLifetime(), false};
		}
		const auto Direction =
		    Session.ResolveFrameSemantic(InContext.Frame, "Engine.Scene.MainDirectionalLightDirection");
		if (!Direction || Direction->Type != FMaterialParameterType::Numeric(EMaterialScalar::Float, 3))
		{
			throw std::invalid_argument("Contact shadows require a scene light direction");
		}
		FContactShadowInputs Inputs;
		Inputs.Depth = Depth;
		Inputs.Normals = InContext.Resources.GBuffer[1];
		Inputs.Surface = InContext.Resources.GBuffer[2];
		Inputs.Mask = Mask;
		Inputs.View = InContext.View;
		Inputs.LightDirection = {std::bit_cast<float>(Direction->Words[0]), std::bit_cast<float>(Direction->Words[1]),
		                         std::bit_cast<float>(Direction->Words[2])};
		Inputs.Settings = Settings;
		auto Pass = MakeContactShadowPass(InContext.Graph, Inputs);
		Pass.Statistics = InContext.FullscreenStatistics;
		AddFullscreenPass(Session, InContext.Graph, std::move(Pass), InContext.bDeferPreparation);
		InContext.Resources.DirectionalVisibility = Mask;
	}
	if (Settings.DebugMode == 2)
	{
		Depth = HierarchicalDepth.Request(Session, InContext.Graph, Request, InContext.bDeferPreparation);
	}
}

void FContactShadowFeature::AddDebug(FRenderFeatureContext& InContext)
{
	const auto& Settings = InContext.Settings.ContactShadows;
	const bool bDepth = Settings.DebugMode == 2;
	if ((!bDepth && (!Settings.DebugMode || !bActive)) || (bDepth && !Depth.Texture))
	{
		return;
	}
	if (bDepth)
	{
		Depth.Validate(InContext.Graph, InContext.View);
	}
	const auto Input =
	    bDepth ? FRenderTargetSource{ERenderTargetKind::Texture, Depth.Texture, Depth.Lifetime, false} : Mask;
	const auto Mip = bDepth ? std::min(Settings.PreviewMip, static_cast<std::uint32_t>(Depth.MipSizes.size() - 1)) : 0;
	const auto Viewport =
	    InContext.View.Viewport.value_or(FViewport{0, 0, float(InContext.View.Width), float(InContext.View.Height)});
	auto Pass = MakeScreenTexturePreview(Input, Viewport, Mip,
	                                     bDepth && InContext.View.DepthConvention == EDepthConvention::Standard);
	Pass.Statistics = InContext.FullscreenStatistics;
	AddFullscreenPass(InContext.Session, InContext.Graph, std::move(Pass), InContext.bDeferPreparation);
}

void FContactShadowFeature::EndFrame(FRenderFeatureContext& InContext)
{
	HierarchicalDepth.EndFrame();
	InContext.Statistics.HierarchicalDepth = HierarchicalDepth.Statistics();
	if (!bActive)
	{
		Mask = {};
	}
}

void FContactShadowFeature::Reset()
{
	HierarchicalDepth = {};
	Depth = {};
	Source = {};
	Mask = {};
	bActive = false;
}

std::uint64_t FContactShadowFeature::ResourceBytes() const
{
	const auto* Texture = Mask.Texture ? Mask.Texture->GetColorTarget() : nullptr;
	return HierarchicalDepth.Statistics().Bytes + (Texture ? std::uint64_t(Texture->Width) * Texture->Height : 0);
}
} // namespace

std::unique_ptr<IRenderFeature> MakeContactShadowFeature()
{
	return std::make_unique<FContactShadowFeature>();
}
} // namespace Hyperion
