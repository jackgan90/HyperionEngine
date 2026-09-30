#pragma once
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneInstance.h"

namespace Hyperion
{
// View state and GPU target ownership. The plugin retains lifecycle and extension composition.
class FEditorViewport
{
public:
	void InitializeCamera(const FSceneInstance& InScene);
	void Resize(FSize InLogical, FSize InPixels, std::uint32_t InLimit, FRenderSession& InSession, FSize InFixed = {});
	void FrameScene(const FSceneInstance& InScene);
	void ResetNavigation();
	FSceneViewRequest MakeViewRequest(const FRenderSettings& InSettings, ESceneCullingMode InCulling,
	                                  const std::optional<FMat4>& InFrozenCulling, bool bInBatching) const;
	FSceneCameraController Navigation{ESceneCameraNavigationMode::Fly};
	FSceneCameraView ViewCamera;
	std::optional<FSceneHandle> PreviewCamera;
	bool bViewportCameraInitialized{};
	FRenderTargetSource ViewportTarget;
	FSize ViewportSize;
	FGuiImageRegion ViewportRegion;
	bool bViewportVisible{};
	bool bCameraDragging{};
};
} // namespace Hyperion
