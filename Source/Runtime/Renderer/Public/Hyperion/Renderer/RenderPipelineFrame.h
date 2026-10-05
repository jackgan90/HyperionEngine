#pragma once
#include "Hyperion/RenderControls/RenderStatistics.h"
#include "Hyperion/Renderer/HierarchicalDepth.h"
#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
struct FFullscreenPreparationStatistics;

// Complete Renderer snapshot; its camera view is never part of the diagnostic wire.
struct FRenderFrameStatistics : FForwardPipelineStatistics
{
	std::optional<FRenderView> MainCameraView;
};

// Captured on Render; read after this frame's RHI graph execution completes.
class FForwardFrame
{
public:
	FRenderFrameStatistics Statistics() const;

private:
	FRenderFrameStatistics Base;
	FRenderViewPreparation Preparation;
	std::shared_ptr<FFullscreenPreparationStatistics> Fullscreen;
	bool bDeferred{};
	friend class FForwardRenderPipeline;
	friend class FSceneRenderPipeline;
};
} // namespace Hyperion
