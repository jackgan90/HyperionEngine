#pragma once
#include "Hyperion/Renderer/HierarchicalDepth.h"
#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
struct FFullscreenPreparationStatistics;

struct FSelectionOutlineStatistics
{
	std::size_t Objects{};
	std::size_t MaskPasses{};
	std::size_t Items{};
	std::size_t PendingItems{};
	std::size_t UnsupportedItems{};
	bool bRejectedPublication{};
};

// Shared pipeline statistics; the original public name remains source compatible.
struct FForwardPipelineStatistics
{
	double PreparationMilliseconds{};
	double ShadowSetupMilliseconds{};
	double FullscreenPreparationMilliseconds{};
	std::uint64_t SceneTargetBytes{};
	std::size_t FullscreenDraws{};
	FSceneVisibilityStats Spatial;
	FLocalLightStatistics LocalLights;
	FSelectionOutlineStatistics SelectionOutline;
	std::vector<FRenderViewStatistics> Views;
	std::uint64_t ShadowTextureBytes{};
	bool bShadows{};
	bool bContactShadows{};
	FHierarchicalDepthStats HierarchicalDepth;
	std::optional<FScenePublicationToken> SceneToken;
	ESceneCameraStatus CameraStatus = ESceneCameraStatus::Active;
	std::optional<FRenderView> MainCameraView;
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
} // namespace Hyperion
