#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include <algorithm>

namespace Hyperion
{
void FSceneRenderPipeline::PrepareClusters(const FRenderView& InView, const FMaterialFrameContext& InFrame,
                                           bool bInEnabled)
{
	FClusterLightFrame Frame{DefaultClusterParameters(), {}};
	if (bInEnabled && InFrame.GetSceneMetadata())
	{
		const FFrustumVisibility Frustum(InView.CullingViewProjection.value_or(InView.ViewProjection));
		const auto Lights = LocalLightIndex.Query(
		    *InFrame.GetSceneMetadata(), InView.CullingMode == ESceneCullingMode::None ? nullptr : &Frustum,
		    InView.CullingMode == ESceneCullingMode::Bvh, LastStatistics.LocalLights);
		Frame = Clusters.Build(InView, Lights);
	}
	else
	{
		Clusters.Reset();
	}
	const bool bSameResources =
	    std::equal(ClusterParameters.begin(), ClusterParameters.end(), Frame.Parameters.begin(), Frame.Parameters.end(),
	               [](const auto& InA, const auto& InB)
	               {
		               return InA.Name == InB.Name && InA.Value.Type == InB.Value.Type &&
		                      (InA.Value.Type.Kind == EMaterialValueKind::Numeric || InA.Value == InB.Value);
	               });
	if (!ClusterLifetime || !bSameResources)
	{
		ClusterLifetime = Session.GetResources().CreateScopeLifetime();
	}
	ClusterParameters = std::move(Frame.Parameters);
	auto& Stats = LastStatistics.LocalLights;
	Stats.ClusterCells = Frame.Statistics.Cells;
	Stats.ClusterOccupied = Frame.Statistics.Occupied;
	Stats.ClusterReferences = Frame.Statistics.References;
	Stats.ClusterMaximum = Frame.Statistics.MaximumLights;
	Stats.ClusterBytes = Frame.Statistics.BufferBytes;
	Stats.ClusterBuildMilliseconds = Frame.Statistics.BuildMilliseconds;
	Stats.bClusterRebuilt = Frame.Statistics.bRebuilt;
}
} // namespace Hyperion
