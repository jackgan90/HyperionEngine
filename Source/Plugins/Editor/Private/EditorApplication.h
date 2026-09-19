#pragma once
#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Renderer/TransformGizmo.h"

namespace Hyperion
{
struct FEditorOptions
{
	std::filesystem::path Mounts;
	std::filesystem::path Layout;
	std::filesystem::path UiPreferences;
	std::optional<float> ApplicationScale;
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
	bool bKernelOnly{};
	std::vector<std::string> DisabledPlugins;
	bool bExercise{};
	bool bExerciseGizmo{};
	bool bExercisePicking{};
};

FEditorOptions ParseEditorOptions(int InCount, char** InValues);

class FEditorPlugin final : public FPlugin
{
public:
	FEditorPlugin(FEditorOptions InOptions, FPluginContext& InContext);
	~FEditorPlugin();
	void Start(FPluginContext& InContext) override;
	void Update(const FPluginUpdate& InUpdate) override;
	void Stop() noexcept override;
	void Finish();

private:
	void Initialize();
	bool AdvanceFrame(float InDelta);
	void LoadCatalogs();
	void Shutdown();
	void OpenScene(const std::string& InPath);
	void ShowOpenScene();
	void DrawMenus();
	void DrawApplicationScale();
	void DrawToolbar();
	void DrawOutliner();
	void DrawNode(FSceneHandle InHandle);
	void DrawDetails();
	void DrawSceneBrowser();
	void DrawOpenDialog();
	void DrawViewport(float InDelta, std::span<const FInputEvent> InEvents);
	void DrawGizmoToolbar();
	void DrawGizmo();
	void DrawGizmoOverlay();
	void UpdateGizmoDrag(const FGuiPointerState& InPointer);
	void FinishGizmo(bool bInCancel = false);
	void ExerciseGizmoInput(std::vector<FInputEvent>& InEvents);
	void ExercisePickingInput(std::vector<FInputEvent>& InEvents);
	void PreparePickingExercise();
	bool ExercisePickingScene(std::vector<FInputEvent>& InEvents);
	void ExercisePickingSelection(std::vector<FInputEvent>& InEvents, FVec2 InCenter, FVec2 InEmpty);
	void ExercisePickingView(std::vector<FInputEvent>& InEvents, FVec2 InCenter);
	void CheckGizmoHistory(unsigned InPhase);
	void ExerciseGizmoFocus(unsigned InPhase, FVec2 InStart, FVec2 InEnd, std::vector<FInputEvent>& InEvents);
	std::string StatusText() const;
	FGuiDrawData DrawGui(float InDelta, std::span<const FInputEvent> InEvents);
	void RouteCamera(float InDelta, std::span<const FInputEvent> InEvents);
	void Render(FGuiDrawData InGui, bool bInCapture);
	void ResizeViewport();
	void RouteViewportPicking(std::span<const FInputEvent> InEvents);
	std::optional<FSceneCameraView> PickingCamera() const;
	void SaveLayout();
	void ExerciseInput(std::vector<FInputEvent>& InEvents);
	void ExerciseDocumentInput(std::vector<FInputEvent>& InEvents);
	void ExerciseViewInput(std::vector<FInputEvent>& InEvents);
	void ExerciseViewHistory();
	void ExerciseViewPreview(std::vector<FInputEvent>& InEvents);
	bool ExerciseTransformInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseTransformHistory(std::vector<FInputEvent>& InEvents);
	bool ExerciseTransformDrag(std::vector<FInputEvent>& InEvents);
	bool ExerciseUnchangedHistory(std::vector<FInputEvent>& InEvents);
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
	void DrawViewSelector(float InWidth);
	void DrawViewOptions();
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
	void CommitEdit(FSceneHandle InHandle, FSceneNode InCandidate, std::uint64_t InExpectedRevision,
	                std::uint64_t InInteraction = 0);
	void FinishInspectorEdit();
	void RouteHistoryShortcuts(std::vector<FInputEvent>& InEvents);
	void Undo();
	void Redo();
	void SaveScene(const std::string& InDestination);
	void PollSave();
	void DrawSaveDialog();
	void DrawDiscardDialog();
	void SelectObject(std::optional<FSceneHandle> InHandle);
	bool PollClose();
	bool IsDirty() const;

	FEditorOptions Options;
	FMountedFileSystem* Files;
	FPluginContext& Context;
	FApplicationControl& Control;
	bool bFinished{};
	FTaskSystem& Tasks;
	FIOService& IO;
	FAssetService& Assets;
	FWindow* Window{};
	IRHIDevice* Device{};
	IRHISwapchain* Swapchain{};
	FShaderCompiler* Compiler{};
	FRenderSession* Session{};
	std::unique_ptr<FSceneRenderPipeline> Pipeline;
	FGui* Gui{};
	FGuiRenderer* GuiRenderer{};
	std::unique_ptr<FSceneInstance> Scene;
	FSceneCameraController Camera{ESceneCameraNavigationMode::Fly};
	FSceneCameraView ViewCamera;
	std::optional<FSceneHandle> PreviewCamera;
	bool bViewportCameraInitialized{};
	bool bViewOptionsOpen{};
	FRenderTargetSource ViewportTarget;
	FSize ViewportSize;
	FGuiImageRegion ViewportRegion;
	FTransformGizmo Gizmo;
	ETransformGizmoMode GizmoMode = ETransformGizmoMode::Position;

	struct FGizmoEdit
	{
		FSceneHandle Handle;
		FMat4 Initial;
		FMat4 Preview;
		std::uint64_t Revision{};
		FVec4 Bounds;
	};

	std::optional<FGizmoEdit> GizmoEdit;
	bool bGizmoUsedMouse{};
	std::array<FVec4, 3> GizmoButtonBounds;
	std::uint32_t GizmoExerciseStep{};
	FMat4 GizmoExerciseBefore;
	FMat4 GizmoExerciseAfter;
	FSceneCameraView GizmoExerciseCamera;
	bool bGizmoVerified{};
	FForwardPipelineStatistics RenderStats;
	FDeviceStats DeviceStats;
	std::vector<std::string> ScenePaths;
	std::optional<FSceneHandle> Selection;
	bool bSelectionInitialized{};
	unsigned PickingExerciseStep{};
	unsigned PickingSceneStep{};
	FVec4 PickingLightBounds;
	FSceneHandle PickingNear;
	FSceneHandle PickingFar;
	FSceneHandle PickingPreview;
	bool bPickingVerified{};

	struct FViewportClick
	{
		FVec2 Start;
		FVec4 Bounds;
		FSize Size;
		FSceneCameraView Camera;
		std::uint64_t Revision{};
	};

	std::optional<FViewportClick> ViewportClick;
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

	struct FInspectorTransaction
	{
		std::uint64_t Interaction{};
		FSceneHandle Handle;
		std::size_t HistoryIndex{};
		std::uint64_t Revision{};
	};

	std::optional<FInspectorTransaction> InspectorTransaction;
	std::uint64_t InspectorInteraction{};

	struct FPendingInspectorEdit
	{
		FSceneHandle Handle;
		FSceneNode Candidate;
		std::uint64_t Revision{};
		std::uint64_t Interaction{};
	};

	std::optional<FPendingInspectorEdit> PendingInspectorEdit;

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
