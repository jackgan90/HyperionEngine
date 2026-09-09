#pragma once
#include "Hyperion/Renderer/CascadedShadowMap.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
struct FForwardPipelineStatistics
{
	double PreparationMilliseconds{};
	double ShadowSetupMilliseconds{};
	FSceneVisibilityStats Spatial;
	std::vector<FRenderViewStatistics> Views;
	std::uint64_t ShadowTextureBytes{};
	bool bShadows{};
};

// Render-owned orchestration: all shadow depth views -> forward -> extension passes.
class FForwardRenderPipeline
{
public:
	explicit FForwardRenderPipeline(FRenderSession& InSession);
	void Build(FRenderGraph& InGraph, FRenderView InMain, std::shared_ptr<const FMaterialFrameContext> InFrame,
	           const FCascadedShadowSettings& InShadows, FVec4 InClear,
	           const std::function<void(FRenderGraph&)>& InExtensions = {});
	const FForwardPipelineStatistics& Statistics() const;
	const FCascadedShadowMap& Shadows() const;

private:
	FRenderSession& Session;
	FCascadedShadowMap ShadowMaps;
	std::shared_ptr<const void> Lifetime;
	std::uint64_t AllocatedShadowBytes{};
	FForwardPipelineStatistics LastStatistics;
};
} // namespace Hyperion
