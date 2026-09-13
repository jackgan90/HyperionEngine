#include "Hyperion/Renderer/LightVolumePass.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"

namespace Hyperion
{
void FSceneRenderPipeline::AddLocalLights(FRenderGraph& InGraph, const FRenderView& InView,
                                          const FMaterialFrameContext& InFrame, bool bInDeferPreparation)
{
	const auto& Metadata = InFrame.GetSceneMetadata();
	if (!Metadata)
	{
		return;
	}
	const FFrustumVisibility Frustum(InView.CullingViewProjection.value_or(InView.ViewProjection));
	FLightVolumePassDesc Pass;
	Pass.Lights = LocalLightIndex.Query(*Metadata, InView.CullingMode == ESceneCullingMode::None ? nullptr : &Frustum,
	                                    InView.CullingMode == ESceneCullingMode::Bvh, LastStatistics.LocalLights);
	if (Pass.Lights.empty())
	{
		return;
	}
	Pass.View = InView;
	Pass.GBuffer = GBuffer;
	Pass.Depth = SceneDepth;
	Pass.Lifetime = Lifetime;
	Pass.Targets = ColorTargets("Deferred/LocalLights", EAttachmentLoad::Load);
	for (const auto& Texture : GBuffer)
	{
		Pass.Targets.Reads.push_back({ERenderTargetKind::Texture, Texture, Lifetime, false});
	}
	Pass.Targets.Reads.push_back({ERenderTargetKind::Texture, SceneDepth, Lifetime, false});
	LastStatistics.LocalLights.Draws = Pass.Lights.size();
	Session.AppendLightVolumes(InGraph, std::move(Pass), bInDeferPreparation);
}
} // namespace Hyperion
