#pragma once
#include "Hyperion/Renderer/CascadedShadowMap.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
struct FFullscreenPreparationStatistics;

struct FForwardPipelineStatistics
{
	double PreparationMilliseconds{};
	double ShadowSetupMilliseconds{};
	double FullscreenPreparationMilliseconds{};
	std::uint64_t SceneTargetBytes{};
	std::size_t FullscreenDraws{};
	FSceneVisibilityStats Spatial;
	std::vector<FRenderViewStatistics> Views;
	std::uint64_t ShadowTextureBytes{};
	bool bShadows{};
	FSceneVisibilityStats MainView() const;
};

// Captured on Render; read after this frame's RHI graph execution completes.
class FForwardFrame
{
public:
	FForwardPipelineStatistics Statistics() const;

private:
	FForwardPipelineStatistics Base;
	FRenderViewPreparation Preparation;
	std::shared_ptr<FFullscreenPreparationStatistics> Fullscreen;
	bool bDeferred{};
	friend class FForwardRenderPipeline;
	friend class FSceneRenderPipeline;
};

// Render-owned orchestration: all shadow depth views -> forward -> extension passes.
class FForwardRenderPipeline
{
public:
	explicit FForwardRenderPipeline(FRenderSession& InSession);
	void Build(FRenderGraph& InGraph, FRenderView InMain, std::shared_ptr<const FMaterialFrameContext> InFrame,
	           const FCascadedShadowSettings& InShadows, FVec4 InClear,
	           const std::function<void(FRenderGraph&)>& InExtensions = {}, bool bInDeferPreparation = false);
	FForwardFrame GetFrame() const; // Render, before building the next frame.
	void Complete();                // Render, after executing a deferred graph.
	const FForwardPipelineStatistics& Statistics() const;
	const FCascadedShadowMap& Shadows() const;

private:
	FRenderSession& Session;
	FCascadedShadowMap ShadowMaps;
	std::shared_ptr<const void> Lifetime;
	std::uint64_t AllocatedShadowBytes{};
	EDepthConvention ShadowDepthConvention = EDepthConvention::Standard;
	FForwardPipelineStatistics LastStatistics;
	bool bPending{};
};
} // namespace Hyperion
