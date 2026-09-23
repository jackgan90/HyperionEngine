#pragma once
#include "AssetEditorWindow.h"
#include "AssetWorkspace.h"
#include "ContentBrowser.h"
#include "EditorHistoryState.h"
#include "EditorInspectionCache.h"
#include "EditorPreferences.h"
#include "EditorSelection.h"
#include "Hyperion/Config/ApplicationClose.h"
#include "Hyperion/Content/ContentRootService.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/Capture/FrameCapture.h"
#endif
#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Renderer/RenderCaptureControl.h"
#include "Hyperion/Renderer/RenderDiagnostics.h"
#include "Hyperion/Renderer/RenderOutput.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneEditTarget.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/Renderer/SelectionOutline.h"
#include "Hyperion/Renderer/TransformGizmo.h"
#include "Hyperion/Renderer/TransientGeometry.h"
#include "Hyperion/Renderer/ViewportPlacement.h"
#include "Hyperion/Scene/ObjectPlacement.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "Hyperion/SceneEditing/SceneDocumentHost.h"
#include "Hyperion/SceneEditing/ScenePlacement.h"
#include <semaphore>

namespace Hyperion
{
struct FEditorOptions
{
	std::filesystem::path EngineContent;
	std::optional<std::filesystem::path> AssetRoot;
	bool bReadOnly{};
	std::filesystem::path Layout;
	std::filesystem::path UiPreferences;
	std::filesystem::path PreferencesPath;
	FEditorPreferences Preferences;
	std::string PreferenceError;
	std::string ExerciseCapture;
	std::optional<float> ApplicationScale;
	std::filesystem::path Capture;
	std::filesystem::path Report;
	std::filesystem::path Benchmark;
	std::filesystem::path ExerciseDocument;
	std::filesystem::path ExerciseViews;
	std::filesystem::path ExercisePlacement;
	std::filesystem::path ExerciseOutlines;
	std::filesystem::path ExerciseContent;
	std::filesystem::path ExerciseAssets;
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
	bool bExerciseMultiSelection{};
};

FEditorOptions ParseEditorOptions(int InCount, char** InValues);

class FEditorPlugin final : public FPlugin,
                            public IContentRootParticipant,
                            public ISceneDocumentHost,
                            public ISceneViewport,
                            public IScenePlacement,
                            public IRenderOutput,
                            public IRenderCaptureControl,
                            public IRenderDiagnostics,
                            public IApplicationClose
{
public:
	FEditorPlugin(FEditorOptions InOptions, FPluginContext& InContext);
	~FEditorPlugin();
	FContentRootParticipantState ContentRootState() const override;
	void ReleaseContentRoot() override;
	void ContentRootChanged() override;
	void Start(FPluginContext& InContext) override;
	void Update(const FPluginUpdate& InUpdate) override;
	void Stop() noexcept override;
	void Finish();
	void OpenDocument(const FSceneOpenRequest& InRequest) override;
	FSceneHostStatus DocumentStatus() const override;
	FSceneHostStatus PollDocument() override;
	bool SupportsContentTransitions() const override;
	FSceneViewportState ViewportState() const override;
	void SetViewportCamera(const FSceneCameraView& InCamera) override;
	void FrameScene() override;
	void SetViewportOptions(const FSceneViewportOptions& InOptions) override;
	void SetViewportSpeed(float InSpeed) override;
	void PreviewSceneCamera(std::optional<FSceneHandle> InHandle) override;
	void SaveInitialView() override;
	void CreateViewCamera() override;
	void ApplyViewToCamera(FSceneHandle InHandle) override;
	FPlacementCatalog PlacementCatalog() const override;
	std::optional<FSceneNodeInfo> PlaceObject(const FScenePlacementRequest& InRequest) override;
	std::shared_ptr<FPendingImageOutput> RequestImage(const FImageOutputRequest& InRequest) override;
	std::optional<FImageArtifact> PollImage(const std::shared_ptr<FPendingImageOutput>& InPending) override;
	FRenderCaptureInfo RenderCaptureInfo() const override;
	void RequestRenderCapture() override;
	void OpenRenderCapture() override;
	void SetRenderCapturePreference(bool bInEnabled) override;
	FRenderDiagnostics RenderDiagnostics() override;
	FApplicationCloseState ApplicationCloseState() const override;
	FApplicationCloseState RequestApplicationClose(const FApplicationCloseRequest& InRequest) override;
	FSceneComponentDiagnostics ComponentDiagnostics(FSceneHandle InHandle, std::string_view InComponent) override;

private:
	std::string CloseState = "idle";
	std::string CloseError;
	void StartSaveBeforeClose(const std::string& InPath);
	void DiscardBeforeClose();
	std::shared_ptr<FPendingImageOutput> PendingImage;

	struct FProjectedLightMarker
	{
		FSceneNodeView View;
		FVec4 Bounds;
		float Depth{};
	};

	void Initialize();
	void LoadSceneDocument(const std::string& InPath, bool bInDiscard);
	bool IsDocumentInteractionBusy() const;
	void InitializeSceneDocument();
	void UpdateDocumentInteraction();
	void EnsureAssetWindow();
	void CloseAssetWindow();
	bool IsAssetWindowBlocked() const;
	void AdvanceAssetWindow(float InDelta, std::vector<FInputEvent> InEvents,
	                        const std::filesystem::path& InCapture = {});
	void ExerciseAssetInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseAssetPreviewInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseAssetPreviewHistory(std::vector<FInputEvent>& InEvents);

	struct FAssetPreviewExercise
	{
		unsigned Step{};
		unsigned Frames{};
		FVec4 Canvas;
		FVec2 Pointer;
		std::string Before;
		std::string After;
		bool bSawPreparing{};
		bool bSawReady{};
	};

	FAssetPreviewExercise AssetPreviewExercise;
	void CheckAssetSaveShortcut();
	void CheckPendingAssetEdit();
	bool bPendingAssetEditChecked{};
	void ExerciseAssetWindowInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowFixture(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowSizing(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowClosing(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowSaving(std::vector<FInputEvent>& InEvents);
	std::uint64_t AssetExerciseFrames{};
	std::shared_ptr<const FBytes> AssetExerciseSavedBytes;
	float AssetExerciseScale{};
	FSceneCameraView AssetExerciseSceneCamera;
	void ExerciseAssetOpening(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetNameInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetSaving(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetPropertyInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetReferences(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetTextureInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWorkspaceInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetDiscardInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetPanelInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetTabClose(std::vector<FInputEvent>& InEvents, const std::string& InPath);
	void ExerciseCustomMaterialInput(std::vector<FInputEvent>& InEvents);
	void ExerciseCustomMaterialReset(std::vector<FInputEvent>& InEvents);
	FVec4 AssetExercisePanelStart{};
	FVec2 AssetExercisePointer{};
	std::shared_ptr<const FSceneModelData> AssetExerciseModel;
	float AssetExerciseRoughness{};
	std::size_t AssetExerciseIndex{};
	std::string AssetExerciseOriginalName;
	std::string ContentRevealPath;
	int AssetExerciseLoggedStep = -1;
	bool bAssetsVerified{};
	void CollectEditorInput(std::vector<FInputEvent>& InEvents, std::vector<FInputEvent>& InAssetEvents);
	FGuiDrawData DrawMainWindow(float InDelta, std::span<const FInputEvent> InEvents, bool bInDrawable);
	bool AdvanceFrame(float InDelta);
	void InitializeContentBrowser();
	void RefreshContent();
	void PollContent();
	void DrawContentTree(const std::string& InPath);
	void DrawContentGrid();
	void DrawAssetMessage();
	void RequestOpenAsset(const std::string& InPath);
	void QueueContentRoot(const std::filesystem::path& InDirectory);
	void ProcessContentRoot();
	void CloseContentDocument();
	void CancelContentRequests();
	void ExerciseContentInput(std::vector<FInputEvent>& InEvents);
	void ExerciseContentSwitch(std::vector<FInputEvent>& InEvents);
	void ExerciseContentBrowser(std::vector<FInputEvent>& InEvents);
	void ExerciseContentFailures(std::vector<FInputEvent>& InEvents);
	void ExerciseContentDismissal(std::vector<FInputEvent>& InEvents);
	void ExerciseContentClose(std::vector<FInputEvent>& InEvents);
	void PrepareContentRoot();
	void CompleteContentRoot();
	void DrawRootMenu();
	void Shutdown();
	void OpenScene(const std::string& InPath);
	void ShowOpenScene();
	void DrawMenus();
	void DrawEditMenu();
	void DrawPreferences();
	void DrawCaptureButton();
	std::string CaptureStatus() const;
	bool CanCapture() const;
	void SavePreferences();
	void ExerciseCaptureInput(std::vector<FInputEvent>& InEvents);
	void DrawWindowMenu();
	void DrawApplicationScale();
	void DrawToolbar();
	void DrawOutliner();
	void DrawNode(FSceneHandle InHandle);
	void DrawDetails();
	void DrawSceneBrowser();
	void InitializePlacement();
	void DrawPlacementPanel();
	void PollPlacementResources();
	void PollPlacementIcons();
	std::string PlacementUnavailableReason(const FPlaceableObject& InObject) const;
	void RoutePlacement();
	void UpdatePlacementPreview(const FPlaceableObject& InObject, const FSceneCameraView& InCamera, FVec2 InPointer);
	void CancelPlacement();
	void CommitPlacement(const FPlaceableObject& InObject, FVec3 InPosition);
	FSceneHandle CommitCreate(FSceneNode InNode);
	std::shared_ptr<const FTransientGeometry> FreezePlacementPreview() const;
	void DrawLightMarkers();
	std::vector<FProjectedLightMarker> CollectLightMarkers() const;
	void DrawLightMarker(const FSceneNode& InNode, const FMat4& InWorld, bool bInSelected);
	std::optional<FSceneHandle> PickLightMarker(FVec2 InPoint) const;
	std::uint64_t LightTexture(const FSceneNode& InNode) const;
	void DrawMainLightAction(FSceneHandle InHandle);
	void DrawOpenDialog();
	void DrawViewport(float InDelta, std::span<const FInputEvent> InEvents);
	void DrawGizmoToolbar();
	void DrawGizmo();
	void DrawGizmoOverlay();
	void UpdateGizmoDrag(const FGuiPointerState& InPointer);
	void FinishGizmo(bool bInCancel = false);
	void ExerciseGizmoInput(std::vector<FInputEvent>& InEvents);
	void ExercisePickingInput(std::vector<FInputEvent>& InEvents);
	void ExerciseOutlines();
	void PrepareOutlineExercise();
	void ExercisePlacementInput(std::vector<FInputEvent>& InEvents);
	bool ExercisePlacementMenu(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementDrag(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementCancel(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementHistory();
	void ExercisePlacementMarkers(std::vector<FInputEvent>& InEvents);
	void CheckPlacementMarkerDraws(const FGuiDrawData& InData) const;
	void ExercisePlacementDocument(std::vector<FInputEvent>& InEvents);
	void PreparePickingExercise();
	bool ExercisePickingScene(std::vector<FInputEvent>& InEvents);
	void ExercisePickingSelection(std::vector<FInputEvent>& InEvents, FVec2 InCenter, FVec2 InEmpty);
	void ExerciseMultiSelection(std::vector<FInputEvent>& InEvents);
	void PrepareMultiSelection();
	void ExerciseMultiSelectionClicks(std::vector<FInputEvent>& InEvents);
	void ExerciseMultiDetails(std::vector<FInputEvent>& InEvents);
	void ExerciseMultiGizmo(std::vector<FInputEvent>& InEvents);
	void ExerciseMultiHistory();
	void ExerciseMultiCancellation();
	void PrepareMultiSelectionMarkers();
	void CheckMultiSelectionMarkerDraws(const FGuiDrawData& InData);
	void ExercisePickingView(std::vector<FInputEvent>& InEvents, FVec2 InCenter);
	void CheckGizmoHistory(unsigned InPhase);
	void ExerciseGizmoFocus(unsigned InPhase, FVec2 InStart, FVec2 InEnd, std::vector<FInputEvent>& InEvents);
	std::string StatusText() const;
	FGuiDrawData DrawGui(float InDelta, std::span<const FInputEvent> InEvents);
	void RouteCamera(float InDelta, std::span<const FInputEvent> InEvents);
	void Render(FGuiDrawData InGui, bool bInCapture);
	FImage ExecuteEditorGraph(FRenderGraph InGraph, FSize InSize, bool bInScreenshot, FNativeSurface InSurface,
	                          bool bInCaptureRdc);
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
	void DrawComponentInspector(const FSceneNodeView& InView);
	void DrawSelectionInspector();
	void DrawSharedComponent(std::span<const FSceneHandle> InTargets, const FSceneComponentDescriptor& InType,
	                         std::uint64_t InRevision);
	bool DrawComponent(const FSceneNodeView& InView, const FSceneComponent& InComponent, std::uint64_t InRevision);
	void CommitEdit(FSceneHandle InHandle, FSceneNode InCandidate, std::uint64_t InExpectedRevision,
	                std::uint64_t InInteraction = 0);
	void FinishInspectorEdit();
	void RouteHistoryShortcuts(std::vector<FInputEvent>& InEvents);
	void RouteDeleteShortcut(std::span<const FInputEvent> InEvents);
	void CommitDelete();
	bool ExerciseDeletionInput(std::vector<FInputEvent>& InEvents);
	void ExerciseDeletionHistory();
	void Undo();
	void Redo();
	void SaveScene(const std::string& InDestination);
	void PollSave();
	void SaveBeforeClose();
	void PollSavedClose();
	void DrawSaveDialog();
	void DrawDiscardDialog();
	void CancelDiscardAction();
	void SelectObject(std::optional<FSceneHandle> InHandle);
	void CommitEdits(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision,
	                 std::uint64_t InInteraction = 0);
	void SetSelection(FEditorSelection InSelection);
	void ClickObject(std::optional<FSceneHandle> InHandle, bool bInToggle);
	void DrawSelectionMarkers();
	void PruneSelection();
	void BeginGizmoEdit(const FSceneNodeView& InView, FVec4 InBounds);
	void PreviewGizmoEdit(const FMat4& InPrimaryLocal);
	std::vector<FSceneHandle> SelectedRoots() const;
	bool PollClose();
	bool IsDirty() const;

	FEditorOptions Options;
	bool bPreferencesDialog{};
	bool bRequestPreferences{};
	bool bCaptureRequested{};
	FVec4 EditMenuBounds;
	FVec4 PreferencesMenuBounds;
	FVec4 CapturePreferenceBounds;
	FVec4 PreferencesCloseBounds;
	FVec4 CaptureButtonBounds;
	bool bInitialCapturePreference{};
#if HYP_ENABLE_RENDERDOC
	FFrameCapture* FrameCapture{};
#endif
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
	std::unique_ptr<FSceneInstanceEditTarget> SceneTarget;
	FSceneEditDocument SceneDocument;
	std::unique_ptr<FAssetWorkspace> AssetWorkspace;
	std::unique_ptr<FAssetEditorWindow> AssetWindow;
	FSceneCameraController Camera{ESceneCameraNavigationMode::Fly};
	FSceneCameraView ViewCamera;
	std::optional<FSceneHandle> PreviewCamera;
	bool bViewportCameraInitialized{};
	bool bViewOptionsOpen{};
	FRenderTargetSource ViewportTarget;
	FSize ViewportSize;
	FGuiImageRegion ViewportRegion;
	FTransformGizmo Gizmo;
	FObjectPlacementRegistry PlacementRegistry;
	FViewportPlacementSession Placement;
	std::string PlacementFilter;
	std::string PlacementCategory = "All";
	bool bShowPlacement = true;
	bool bFocusPlacement{};
	bool bShowLightMarkers = true;
	bool bPlacementUsedMouse{};
	std::string PlacementStatus;

	struct FPlacementModel
	{
		std::string Asset;
		std::shared_ptr<const FSceneModelData> Data;
		std::shared_ptr<const FRenderResource> Resource;
		std::string Error;
	};

	std::map<std::string, FPlacementModel> PlacementModels;

	struct FPlacementIcon
	{
		std::uint64_t Texture{};
		TAssetRequest<FTextureAsset> Request;
		FRenderTargetSource Source;
		FRenderTargetSource PendingSource;
		std::string Error;
		bool bComplete{};
	};

	std::map<std::string, FPlacementIcon> PlacementIcons;
	std::shared_ptr<const FRenderMaterial> PlacementMaterial;
	std::shared_ptr<const void> PlacementLifetime;
	std::optional<FSceneHandle> PlacementPublication;
	FMat4 PlacementPublicationLocal;
	std::shared_ptr<const FTransientGeometry> PlacementPublicationPreview;
	std::uint32_t PlacementExerciseStep{};
	std::uint32_t PlacementMenuStep{};
	std::uint32_t PlacementExerciseType{};
	std::uint32_t PlacementCancelCase{};
	std::uint32_t PlacementMarkerCase{};
	std::uint32_t PlacementMarkerStep{};
	std::array<FMat4, 2> PlacementMarkerTransforms;
	std::size_t PlacementExerciseBaseNodes{};
	std::size_t PlacementExerciseBaseHistory{};
	std::uint64_t PlacementExerciseBaseState{};
	FVec3 PlacementExercisePosition;
	std::vector<std::string> PlacementExerciseIds;
	std::filesystem::path PlacementCapture;
	bool bPlacementVerified{};
	ETransformGizmoMode GizmoMode = ETransformGizmoMode::Position;

	struct FGizmoTarget
	{
		FSceneHandle Handle;
		FMat4 Initial;
		FMat4 World;
		FMat4 ParentInverse;
		FMat4 Preview;
	};

	struct FGizmoEdit
	{
		FSceneHandle Handle;
		FMat4 Initial;
		FMat4 Preview;
		std::uint64_t Revision{};
		FVec4 Bounds;
		std::vector<FGizmoTarget> Targets;
		bool bGroup{};
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
	FEditorSelection& Selection = SceneDocument.Selection();
	FSelectionOutlineSettings OutlineSettings;
	std::vector<FSceneHandle> OutlineExerciseObjects;
	FSceneHandle OutlineExerciseWall;
	std::filesystem::path OutlineCapture;
	unsigned OutlineExerciseStep{};
	unsigned OutlineExerciseWait{};
	bool bOutlinesVerified{};
	bool bSelectionInitialized{};
	unsigned MultiSelectionStep{};
	unsigned MultiSelectionWait{};
	bool bMultiSelectionVerified{};
	std::vector<FSceneHandle> MultiSelectionObjects;
	std::map<std::string, FVec4> MultiSelectionRows;
	std::array<FMat4, 2> MultiSelectionInitial;
	std::array<FMat4, 2> MultiSelectionFinal;
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
		bool bToggle{};
		FVec4 Bounds;
		FSize Size;
		FSceneCameraView Camera;
		std::uint64_t Revision{};
	};

	std::optional<FViewportClick> ViewportClick;
	std::string OpenPath;
	std::unique_ptr<FContentBrowser> Browser;
	std::optional<TAsyncResult<FAssetHeader>> PendingAssetOpen;
	FCancellationToken AssetOpenCancellation;
	std::string AssetOpenPath;
	std::string AssetMessage;
	bool bAssetMessage{};
	bool bRequestAssetMessage{};
	bool bRevealContentTree{};
	bool bRequestRootDialog{};
	std::filesystem::path RequestedRoot;
	std::optional<FContentRootCandidate> PendingRoot;
	bool bDiscardRoot{};
	bool bSaveThenSwitch{};
	bool bCommitRoot{};
	bool bContentVerified{};
	FVec4 SaveSwitchBounds;
	FVec4 DiscardChangesBounds;
	FVec4 CancelChangesBounds;
	FVec4 DiscardTitleBounds;
	std::shared_ptr<std::binary_semaphore> ContentSaveGate;
	std::map<std::string, FVec4> ContentTileBounds;
	FVec4 ContentClickBounds;
	const std::string& CurrentPath = SceneDocument.GetState().Path;
	std::string Error;
	std::string Filter;
	std::string SceneFilter;
	std::string CatalogError;
	bool bShowViewport = true;
	bool bShowOutliner = true;
	bool bOutlinerToggle{};
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

	using FNodeHistory = FEditorNodeHistory;
	using FHistoryEntry = FEditorHistoryEntry;

	unsigned DeletionExerciseStep{};
	FSceneHandle DeletionExerciseHandle;
	std::string DeletionExerciseId;

	const std::vector<FHistoryEntry>& History = SceneDocument.GetState().History;
	const std::size_t& HistoryCursor = SceneDocument.GetState().HistoryCursor;
	const std::uint64_t& DocumentState = SceneDocument.GetState().State;
	const std::uint64_t& SavedState = SceneDocument.GetState().SavedState;
	const std::uint64_t& DocumentEpoch = SceneDocument.GetState().Epoch;

	const std::optional<FSceneInspectorTransaction>& InspectorTransaction = SceneDocument.GetState().Interaction;
	std::uint64_t InspectorInteraction{};

	struct FPendingInspectorEdit
	{
		FSceneHandle Handle;
		FSceneNode Candidate;
		std::uint64_t Revision{};
		std::uint64_t Interaction{};
		std::vector<FSceneNodeEdit> Edits;
	};

	std::optional<FPendingInspectorEdit> PendingInspectorEdit;
	FEditorInspectionCache InspectorDrafts;

	const std::optional<FScenePendingSave>& PendingSave = SceneDocument.GetState().Save;
	std::string SavePath;
	std::string SaveStatus;
	double LastSaveMilliseconds{};
	bool bSaveDialog{};
	bool bRequestSaveDialog{};
	bool bDiscardDialog{};
	bool bRequestDiscard{};
	bool bPendingClose{};
	bool bSaveThenClose{};
	std::string PendingOpen;
	std::map<std::string, FVec4> InspectionBounds;
	FSceneNode ExerciseOriginal;
	std::uint32_t TransformExerciseStep{};
	std::uint64_t TransformDragReadyAt{};
	FMat4 ExerciseTransformResult;
	bool bDocumentVerified{};
	bool bViewsVerified{};
	FSceneCameraView ExerciseInitialView;
	FSceneCameraView ExerciseEditorView;
};
} // namespace Hyperion
