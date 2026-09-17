#pragma once
#include "Hyperion/Renderer/CascadedShadowMap.h"
#include "Hyperion/Renderer/RenderPipelineFrame.h"

namespace Hyperion
{
// Render-owned orchestration: all shadow depth views -> forward -> extension passes.
class FForwardRenderPipeline
{
public:
	explicit FForwardRenderPipeline(FRenderSession& InSession);
	void Build(FRenderGraph& InGraph, FRenderView InMain, std::shared_ptr<const FMaterialFrameContext> InFrame,
	           const FCascadedShadowSettings& InShadows, FVec4 InClear,
	           const std::function<void(FRenderGraph&)>& InExtensions = {}, bool bInDeferPreparation = false);
	void Build(FRenderGraph& InGraph, const FSceneViewRequest& InRequest, std::shared_ptr<const FSceneFrameSeed> InSeed,
	           const FCascadedShadowSettings& InShadows, FVec4 InClear,
	           const std::function<void(FRenderGraph&)>& InExtensions = {}, bool bInDeferPreparation = false);
	FForwardFrame GetFrame() const; // Render, before building the next frame.
	void Complete();                // Render, after executing a deferred graph.
	const FForwardPipelineStatistics& Statistics() const;
	const FCascadedShadowMap& Shadows() const;

private:
	void BuildResolved(FRenderGraph& InGraph, FRenderView InMain, std::shared_ptr<const FMaterialFrameContext> InFrame,
	                   const FCascadedShadowSettings& InShadows, FVec4 InClear,
	                   const std::function<void(FRenderGraph&)>& InExtensions, bool bInDeferPreparation);
	FRenderSession& Session;
	FCascadedShadowMap ShadowMaps;
	std::shared_ptr<const void> Lifetime;
	std::uint64_t AllocatedShadowBytes{};
	EDepthConvention ShadowDepthConvention = EDepthConvention::Standard;
	FForwardPipelineStatistics LastStatistics;
	bool bPending{};
};
} // namespace Hyperion
