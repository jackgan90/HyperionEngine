#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include <algorithm>

namespace Hyperion
{
void FSceneRenderPipeline::AddContactShadows(FRenderGraph& InGraph, const FRenderView& InView,
                                             const FMaterialFrameContext& InFrame, FVec3 InDirection,
                                             bool bInDeferPreparation)
{
	const auto Direct = Session.ResolveFrameSemantic(InFrame, "Engine.Scene.MainDirectionalLightColor");
	const bool bDirectional = Direct && Direct->Words != FMaterialValue::Float(FVec3{}).Words;
	LastStatistics.bContactShadows =
	    Settings.ContactShadows.bEnabled && bDirectional && (!InFrame.GetSceneToken() || InFrame.CastsSceneShadows());
	const FHierarchicalDepthRequest Request{{ERenderTargetKind::Texture, SceneDepth, Lifetime, false}, InView};
	if (LastStatistics.bContactShadows)
	{
		ContactDepth = HierarchicalDepth.Request(Session, InGraph, Request, bInDeferPreparation);
		if (!ContactMask)
		{
			ContactLifetime = Session.GetResources().CreateScopeLifetime();
			ContactMask = std::make_shared<const FMaterialTextureSource>(
			    FMaterialColorTexture{Width, Height, EMaterialColorFormat::R8Unorm});
		}
		FContactShadowInputs Inputs;
		Inputs.Depth = ContactDepth;
		Inputs.Normals = {ERenderTargetKind::Texture, GBuffer[1], Lifetime, false};
		Inputs.Surface = {ERenderTargetKind::Texture, GBuffer[2], Lifetime, false};
		Inputs.Mask = {ERenderTargetKind::Texture, ContactMask, ContactLifetime, false};
		Inputs.View = InView;
		Inputs.LightDirection = InDirection;
		Inputs.Settings = Settings.ContactShadows;
		auto Pass = MakeContactShadowPass(InGraph, Inputs);
		Pass.Statistics = FullscreenStatistics;
		AddFullscreenPass(Session, InGraph, std::move(Pass), bInDeferPreparation);
	}
	if (Settings.ContactShadows.DebugMode == 2)
	{
		ContactDepth = HierarchicalDepth.Request(Session, InGraph, Request, bInDeferPreparation);
	}
}

void FSceneRenderPipeline::AddContactDebug(FRenderGraph& InGraph, const FRenderView& InView,
                                           bool bInDeferPreparation) const
{
	const bool bDepth = Settings.ContactShadows.DebugMode == 2;
	if ((!bDepth && (!Settings.ContactShadows.DebugMode || !LastStatistics.bContactShadows)) ||
	    (bDepth && !ContactDepth.Texture))
	{
		return;
	}
	if (bDepth)
	{
		ContactDepth.Validate(InGraph, InView);
	}
	const FRenderTargetSource Source{ERenderTargetKind::Texture, bDepth ? ContactDepth.Texture : ContactMask,
	                                 bDepth ? ContactDepth.Lifetime : ContactLifetime, false};
	const auto Mip = bDepth ? std::min(Settings.ContactShadows.PreviewMip,
	                                   static_cast<std::uint32_t>(ContactDepth.MipSizes.size() - 1))
	                        : 0;
	const auto Viewport = InView.Viewport.value_or(FViewport{0, 0, float(Width), float(Height)});
	auto Pass =
	    MakeScreenTexturePreview(Source, Viewport, Mip, bDepth && InView.DepthConvention == EDepthConvention::Standard);
	Pass.Statistics = FullscreenStatistics;
	AddFullscreenPass(Session, InGraph, std::move(Pass), bInDeferPreparation);
}
} // namespace Hyperion
