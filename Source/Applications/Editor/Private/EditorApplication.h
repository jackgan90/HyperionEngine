#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"

namespace Hyperion
{
struct FEditorOptions
{
	std::filesystem::path Mounts;
	std::filesystem::path Layout;
	std::filesystem::path Capture;
	std::filesystem::path Report;
	std::string Scene;
	std::uint32_t Frames{};
	bool bHidden{};
	bool bExercise{};
};

FEditorOptions ParseEditorOptions(int InCount, char** InValues);

class FEditorApplication
{
public:
	explicit FEditorApplication(FEditorOptions InOptions);
	~FEditorApplication();
	void Run();

private:
	void Initialize();
	void LoadCatalogs();
	void Shutdown();
	void OpenScene(const std::string& InPath);
	void ShowOpenScene();
	void DrawMenus();
	void DrawToolbar();
	void DrawOutliner();
	void DrawNode(FSceneHandle InHandle);
	void DrawDetails();
	void DrawSceneBrowser();
	void DrawOpenDialog();
	void DrawViewport();
	std::string StatusText() const;
	FGuiDrawData DrawGui(float InDelta, std::span<const FInputEvent> InEvents);
	void RouteCamera(float InDelta, std::span<const FInputEvent> InEvents);
	void Render(FGuiDrawData InGui, bool bInCapture);
	void ResizeViewport();
	void SaveLayout();
	void ExerciseInput(std::vector<FInputEvent>& InEvents);
	void ExerciseCamera(std::vector<FInputEvent>& InEvents);
	void ExerciseMovement(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX, float InY);
	void ExerciseWheel(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX, float InY);
	void ExerciseClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds);
	void WriteReport();

	FEditorOptions Options;
	std::shared_ptr<FMountedFileSystem> Files;
	FTaskSystem Tasks{4, 1};
	FIOService IO;
	FAssetService Assets;
	std::unique_ptr<FWindow> Window;
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FShaderCompiler> Compiler;
	std::unique_ptr<FRenderSession> Session;
	std::unique_ptr<FSceneRenderPipeline> Pipeline;
	std::unique_ptr<FGui> Gui;
	std::unique_ptr<FGuiRenderer> GuiRenderer;
	std::unique_ptr<FSceneInstance> Scene;
	FSceneCameraController Camera{ESceneCameraNavigationMode::Fly};
	FRenderTargetSource ViewportTarget;
	FSize ViewportSize;
	FGuiImageRegion ViewportRegion;
	FForwardPipelineStatistics RenderStats;
	FDeviceStats DeviceStats;
	std::vector<std::string> ScenePaths;
	std::optional<FSceneHandle> Selection;
	std::string OpenPath;
	std::string CurrentPath;
	std::string Error;
	std::string Filter;
	std::string SceneFilter;
	std::string CatalogError;
	bool bShowViewport = true;
	bool bShowOutliner = true;
	bool bShowDetails = true;
	bool bShowBrowser = true;
	bool bOpenDialog{};
	bool bRequestOpen{};
	bool bResetLayout{};
	bool bViewportVisible{};
	bool bCameraDragging{};
	bool bStopped{};
	bool bReadyLogged{};
	std::uint32_t FrameCount{};
	std::uint32_t ReadyFrames{};
	std::uint32_t OpenCount{};
	float Exposure = 2.5f;

	// Actual widget bounds feed the same normalized event path as Platform input in acceptance mode.
	FVec4 FileMenuBounds;
	FVec4 OpenMenuBounds;
	FVec4 SponzaBounds;
	FVec4 OpenButtonBounds;
	FVec4 CancelButtonBounds;
	std::uint32_t ExerciseStep{};
	std::uint32_t ExerciseWait{};
	std::uint32_t ExerciseMovementStep{};
	std::uint32_t ExerciseWheelStep{};
	float ExerciseSpeed{};
	bool bExerciseMouseDown{};
	bool bMovementVerified{};
	bool bMovementGateVerified{};
	bool bRightReleaseVerified{};
	bool bLookVerified{};
	bool bDollyVerified{};
	bool bSpeedVerified{};
	bool bInputIsolationVerified{};
	bool bLoadErrorObserved{};
	FSceneCameraPose ExercisePose;
};
} // namespace Hyperion
