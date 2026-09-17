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
	std::filesystem::path Benchmark;
	std::filesystem::path ExerciseDocument;
	std::filesystem::path ExerciseViews;
	std::string Scene;
	std::uint32_t Frames{};
	std::uint32_t BenchmarkWarmup = 120;
	std::uint32_t BenchmarkSamples = 300;
	bool bBenchmarkCamera{};
	bool bBenchmarkCollapsed{};
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
	bool AdvanceFrame(float InDelta);
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
	void ExerciseDocumentInput(std::vector<FInputEvent>& InEvents);
	void ExerciseViewInput(std::vector<FInputEvent>& InEvents);
	void ExerciseViewHistory();
	void ExerciseViewPreview(std::vector<FInputEvent>& InEvents);
	bool ExerciseTransformInput(std::vector<FInputEvent>& InEvents);
	void ExerciseCamera(std::vector<FInputEvent>& InEvents);
	void ExerciseMovement(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX, float InY);
	void ExerciseWheel(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX, float InY);
	void ExerciseClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds);
	void WriteReport();
	void BenchmarkCamera();
	void RecordBenchmark();
	void SaveBenchmark();
	void ResetDocument();
	void InitializeViewportCamera();
	void DrawViewControls();
	void DrawCameraActions(FSceneHandle InHandle);
	void SetPreviewCamera(std::optional<FSceneHandle> InHandle);
	bool IsPreviewAvailable() const;
	void SetInitialView();
	void ApplyEditorView(FSceneHandle InHandle);
	void CreateCameraFromView();
	void CommitSettings(FSceneSettings InSettings);
	void RestoreHistory(std::size_t InIndex, bool bInAfter);
	void RemapHistoryHandle(FSceneHandle InBefore, FSceneHandle InAfter);
	void DrawComponentInspector(const FSceneNodeView& InView);
	bool DrawComponent(const FSceneNodeView& InView, const FSceneComponent& InComponent, std::uint64_t InRevision);
	void CommitEdit(FSceneHandle InHandle, FSceneNode InCandidate, std::uint64_t InExpectedRevision);
	void Undo();
	void Redo();
	void SaveScene(const std::string& InDestination);
	void PollSave();
	void DrawSaveDialog();
	void DrawDiscardDialog();
	void SelectObject(FSceneHandle InHandle);
	bool HasDrafts() const;
	bool PollClose();
	bool IsDirty() const;

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
	FSceneCameraView ViewCamera;
	std::optional<FSceneHandle> PreviewCamera;
	bool bViewportCameraInitialized{};
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

	struct FBenchmarkSample
	{
		double FrameMilliseconds{};
		double SceneMilliseconds{};
		double GuiMilliseconds{};
		double RenderMilliseconds{};
		FForwardPipelineStatistics Pipeline;
		std::size_t Nodes{};
	};

	FBenchmarkSample BenchmarkFrame;
	std::vector<FBenchmarkSample> BenchmarkSamples;
	std::uint64_t BenchmarkStarted{};
	double LoadMilliseconds{};

	struct FHistoryEntry
	{
		FSceneHandle Handle;
		std::optional<FSceneNode> Before;
		std::optional<FSceneNode> After;
		FSceneSettings BeforeSettings;
		FSceneSettings AfterSettings;
		std::uint64_t BeforeState{};
		std::uint64_t AfterState{};
	};

	std::vector<FHistoryEntry> History;
	std::size_t HistoryCursor{};
	std::uint64_t NextDocumentState{};
	std::uint64_t DocumentState{};
	std::uint64_t SavedState{};
	std::uint64_t DocumentEpoch{};

	struct FInspectionDraft
	{
		FRecordDraft Record;
		std::uint64_t Revision{};
		bool bModified{};
	};

	std::optional<FSceneHandle> InspectedObject;
	std::map<std::string, FInspectionDraft> InspectorDrafts;

	struct FPendingSave
	{
		TAsyncResult<bool> Result;
		std::string Destination;
		std::uint64_t Epoch{};
		std::uint64_t State{};
		std::uint64_t Started{};
	};

	std::optional<FPendingSave> PendingSave;
	std::string SavePath;
	std::string SaveStatus;
	double LastSaveMilliseconds{};
	bool bSaveDialog{};
	bool bRequestSaveDialog{};
	bool bDiscardDialog{};
	bool bRequestDiscard{};
	bool bPendingClose{};
	std::string PendingOpen;
	std::map<std::string, FVec4> InspectionBounds;
	FSceneNode ExerciseOriginal;
	std::uint32_t TransformExerciseStep{};
	FMat4 ExerciseTransformResult;
	bool bDocumentVerified{};
	bool bViewsVerified{};
	FSceneCameraView ExerciseInitialView;
	FSceneCameraView ExerciseEditorView;
};
} // namespace Hyperion
