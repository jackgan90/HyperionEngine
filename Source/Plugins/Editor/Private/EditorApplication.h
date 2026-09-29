#pragma once
#include "AssetEditorWindow.h"
#include "AssetImportPanel.h"
#include "AssetWorkspace.h"
#include "ContentBrowser.h"
#include "EditorHistoryState.h"
#include "EditorInspectionCache.h"
#include "EditorPreferences.h"
#include "EditorSelection.h"
#include "Hyperion/Config/ApplicationClose.h"
#include "Hyperion/Content/ContentRootService.h"
#include "Hyperion/Core/Logging/LogHistory.h"
#include "Hyperion/Core/ProfilingControl.h"
#include "Hyperion/Core/ProfilingSession.h"
#include "Hyperion/Renderer/RenderBenchmark.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneLightControls.h"
#include "PlacementService.h"
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
	FLogHistory* LogHistory{};
	bool bExerciseLog{};
	std::filesystem::path EngineContent;
	std::optional<std::filesystem::path> AssetRoot;
	bool bReadOnly{};
	std::filesystem::path Layout;
	std::filesystem::path UiPreferences;
	std::filesystem::path PreferencesPath;
	std::filesystem::path RenderSettingsPath;
	FRenderSettings Rendering;
	FEditorPreferences Preferences;
	std::string PreferenceError;
	std::string ExerciseCapture;
	std::optional<float> ApplicationScale;
	std::filesystem::path Capture;
	std::filesystem::path Report;
	std::filesystem::path Benchmark;
	std::filesystem::path ExerciseDocument;
	std::filesystem::path ExerciseViews;
	std::filesystem::path ExerciseRenderControls;
	std::filesystem::path ExercisePlacement;
	std::filesystem::path ExerciseModelPlacement;
	std::filesystem::path ExerciseReparent;
	std::filesystem::path ExerciseOutlines;
	std::filesystem::path ExerciseContent;
	std::filesystem::path ExerciseImport;
	std::filesystem::path ExerciseAssets;
	std::string Scene;
	std::uint32_t Frames{};
	std::uint32_t BenchmarkWarmup = 120;
	std::uint32_t BenchmarkSamples = 300;
	bool bBenchmarkCamera{};
	float BenchmarkCameraStep = .1f;
	FProfilingOptions Profiling;
	FSize BenchmarkViewport;
	bool bBenchmarkLight{};
	ESceneCullingMode CullingMode = ESceneCullingMode::Bvh;
	bool bInstanceBatching = true;
	bool bBenchmarkCollapsed{};
	bool bHidden{};
	bool bKernelOnly{};
	std::vector<std::string> DisabledPlugins;
	bool bExercise{};
	bool bExerciseGizmo{};
	bool bExercisePicking{};
	bool bExerciseMultiSelection{};
	bool bExerciseClipboard{};
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
                            public IRenderSettings,
                            public IShadowControls,
                            public ISceneLightControls,
                            public IProfilingControl,
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
	std::optional<FSceneNodeInfo> PlaceModel(const FSceneModelPlacementRequest& InRequest) override;
	std::shared_ptr<FPendingImageOutput> RequestImage(const FImageOutputRequest& InRequest) override;
	std::optional<FImageArtifact> PollImage(const std::shared_ptr<FPendingImageOutput>& InPending) override;
	FRenderCaptureInfo RenderCaptureInfo() const override;
	void RequestRenderCapture() override;
	void OpenRenderCapture() override;
	void SetRenderCapturePreference(bool bInEnabled) override;
	FRenderCaptureHudInfo RenderCaptureHudInfo() const override;
	void SetRenderCaptureHudPreference(bool bInEnabled) override;
	FRenderDiagnostics RenderDiagnostics() override;
	FRenderHealth RenderHealth() override;
	FSceneLightingInfo LightingInfo() override;
	FRenderSettingsState RenderSettings() const override;
	void SetRenderSettings(std::uint64_t InRevision, const FRenderSettings& InSettings) override;
	void SaveRenderSettings(const std::filesystem::path& InPath) override;
	FCascadedShadowSettings ShadowControls() const override;
	void SetShadowControls(const FCascadedShadowSettings& InSettings) override;
	void ChangeProfiling(std::optional<std::uint32_t> InMask, std::optional<bool> InSampling) override;
	FApplicationCloseState ApplicationCloseState() const override;
	FApplicationCloseState RequestApplicationClose(const FApplicationCloseRequest& InRequest) override;
	FSceneComponentDiagnostics ComponentDiagnostics(FSceneHandle InHandle, std::string_view InComponent) override;

private:
	FRenderSettings Rendering;
	std::uint64_t RenderSettingsRevision = 1;
	bool bShowRenderSettings{};
	bool bShowStatusHud{};
	bool bShowProfilingHud{};
	std::uint32_t ProfilingCategories = 1;
	FRenderDiagnostics HudDiagnostics;
	std::uint64_t HudUpdated{};
	bool bRenderControlsVerified{};
	void ExerciseRenderControlsInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseLightPriorityInput(std::vector<FInputEvent>& InEvents);
	void ExerciseProfilingHudInput(std::vector<FInputEvent>& InEvents);
	void ExerciseProfilingDetailsInput(std::vector<FInputEvent>& InEvents);
	void CheckRenderControlsHud() const;
	ESceneCullingMode CullingMode = ESceneCullingMode::Bvh;
	std::optional<FMat4> FrozenCullingView;
	bool bInstanceBatching = true;
	bool bModelBounds{};
	bool bLightBounds{};
	void DrawRenderSettings();
	void DrawProfilingOptions();
	void DrawProfilingCollection();
	void DrawViewportHud();
	void DrawHudButtons();
	void DrawVisualizationControls();
	std::string RenderSettingsError;
	std::string ProfilingError;
	double FrameIntervalMilliseconds{};
	void DrawDebugBounds();
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
	std::vector<FAssetRef> AssetReferenceCandidates(std::string_view InTypeId) const;
	void DrawAssetMessage();
	void RequestOpenAsset(const std::string& InPath);
	void QueueContentRoot(const std::filesystem::path& InDirectory);
	void ProcessContentRoot();
	void CloseContentDocument();
	void CancelContentRequests();
	void ExerciseContentInput(std::vector<FInputEvent>& InEvents);
	void ExerciseContentSaveAs(std::vector<FInputEvent>& InEvents);
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
	void BeginReparentGesture(FSceneHandle InHandle, FVec4 InBounds);
	void UpdateReparentGesture(std::span<const FInputEvent> InEvents);
	void CancelReparentGesture();
	void DrawReparentTarget(std::optional<FSceneHandle> InParent);
	void DrawReparentRoot();
	void FinishReparentGesture();
	void RouteReparentRow(FSceneHandle InHandle, bool bInActivated = false);
	void DrawPreferences();
	void DrawCaptureButton();
	std::string CaptureStatus() const;
	bool CanCapture() const;
	void SavePreferences();
	void ExerciseCaptureInput(std::vector<FInputEvent>& InEvents);
	void ExerciseCaptureHudInput(std::vector<FInputEvent>& InEvents);
	void DrawWindowMenu();
	void DrawLog();
	void ExerciseLogInput(std::vector<FInputEvent>& InEvents);
	bool bShowLog{};
	bool bFocusLog{};
	bool bLogVerified{};
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
	std::string PlacementUnavailableReason(const FPlacementCandidate& InCandidate) const;
	std::optional<FSceneNodeInfo> PollPlacement(const FPlacementCandidate& InCandidate, FVec3 InPosition);
	FPlacementCandidate ResolvePlacementPayload(const FGuiDragPayload& InPayload);
	void RoutePlacementPayload(const FGuiDragPayload& InPayload, const std::optional<FGuiDragPayload>& InDrop);
	void RoutePlacement();
	void ExerciseModelPlacement(std::vector<FInputEvent>& InEvents);
	void ExerciseModelDrag(std::vector<FInputEvent>& InEvents);
	void CompleteModelDrag(std::vector<FInputEvent>& InEvents, FVec2 InSource);
	void ExerciseModelPlacementHistory();
	void UpdatePlacementPreview(const FPlacementCandidate& InObject, const FSceneCameraView& InCamera, FVec2 InPointer);
	void CancelPlacement();
	void CommitPlacement(const FPlaceableObject& InObject, FVec3 InPosition);
	void CommitPlacement(const FPlacementCandidate& InCandidate, FVec3 InPosition);
	FSceneHandle CommitCreate(FSceneNode InNode);
	std::shared_ptr<const FTransientGeometry> FreezePlacementPreview() const;
	void DrawLightMarkers();
	std::vector<FProjectedLightMarker> CollectLightMarkers() const;
	void DrawLightMarker(const FSceneNode& InNode, const FMat4& InWorld, bool bInSelected);
	std::optional<FSceneHandle> PickLightMarker(FVec2 InPoint) const;
	std::uint64_t LightTexture(const FSceneNode& InNode) const;
	void InspectLightProperty(const FSceneNodeView& InView, const FSceneComponent& InComponent,
	                          std::string_view InField, FPropertyPresentation& InOutPresentation) const;
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
	void ExercisePlacedClipboard(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementCancel(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementHistory();
	void ExercisePlacedSkyActivation();
	void ExercisePlacementMarkers(std::vector<FInputEvent>& InEvents);
	void CheckPlacementMarkerDraws(const FGuiDrawData& InData) const;
	void ExercisePlacementDocument(std::vector<FInputEvent>& InEvents);
	void PreparePickingExercise();
	bool ExercisePickingScene(std::vector<FInputEvent>& InEvents);
	void ExercisePickingSelection(std::vector<FInputEvent>& InEvents, FVec2 InCenter, FVec2 InEmpty);
	void ExerciseMultiSelection(std::vector<FInputEvent>& InEvents);
	void ExerciseReparent(std::vector<FInputEvent>& InEvents);
	void PrepareReparentExercise();
	void ExerciseReparentKeyboard(std::vector<FInputEvent>& InEvents);
	void ExerciseReparentSelection(std::vector<FInputEvent>& InEvents);
	void ExerciseReparentDrag(std::vector<FInputEvent>& InEvents);
	void ExerciseReparentInterruption(std::vector<FInputEvent>& InEvents);
	void VerifyReparentExercise();
	unsigned ReparentExerciseStep{};
	unsigned ReparentExerciseCase{};
	unsigned ReparentSelectionStep{};
	unsigned ReparentKeyboardStep{};
	bool bReparentVerified{};
	std::vector<FSceneHandle> ReparentExerciseNodes;
	std::vector<std::string> ReparentExerciseIds;
	std::vector<FMat4> ReparentExerciseWorlds;
	std::uint64_t ReparentExerciseRevision{};
	std::size_t ReparentExerciseHistory{};
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
	void ExerciseImportInput(std::vector<FInputEvent>& InEvents);
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
	void DrawObjectMetadata(const FSceneNodeView& InView);
	void DrawComponentInspector(const FSceneNodeView& InView);
	void DrawSelectionInspector();
	void DrawSharedComponent(std::span<const FSceneHandle> InTargets, const FSceneComponentDescriptor& InType,
	                         std::uint64_t InRevision);
	bool DrawComponent(const FSceneNodeView& InView, const FSceneComponent& InComponent, std::uint64_t InRevision);
	void CommitEdit(FSceneHandle InHandle, FSceneNode InCandidate, std::uint64_t InExpectedRevision,
	                std::uint64_t InInteraction = 0);
	void FinishInspectorEdit();
	void RouteHistoryShortcuts(std::vector<FInputEvent>& InEvents);
	void RouteClipboardShortcuts(std::span<const FInputEvent> InEvents);
	void ExerciseClipboard(std::vector<FInputEvent>& InEvents);
	void PrepareClipboardExercise();
	void ExerciseClipboardHistory(std::vector<FInputEvent>& InEvents);
	void ExerciseClipboardText(std::vector<FInputEvent>& InEvents);
	unsigned ClipboardExerciseStep{};
	unsigned ClipboardExerciseWait{};
	std::size_t ClipboardExerciseCount{};
	bool bClipboardVerified{};
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
	FVec4 CaptureHudPreferenceBounds;
	FVec4 PreferencesCloseBounds;
	FVec4 CaptureButtonBounds;
	bool bInitialCapturePreference{};
	bool bInitialCaptureHudPreference{};
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

	FPlacementService PlacementService;
	std::map<std::string, FPlacementModel>& PlacementModels = PlacementService.Models;
	std::optional<FPlacementCandidate> PlacementCandidate;
	std::string PlacementSourceType;
	std::string PlacementSourceValue;

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
	std::uint64_t PlacementPublicationRevision{};
	bool bPlacementPublicationSourceMaterials{};
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
	bool bModelPlacementVerified{};
	unsigned ModelPlacementStep{};
	unsigned ModelPlacementCase{};
	std::size_t ModelPlacementBaseHistory{};
	std::size_t ModelPlacementBaseNodes{};
	FVec3 ModelPlacementPosition;
	std::vector<std::string> ModelPlacementIds;
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

	struct FReparentGesture
	{
		std::string Token;
		std::string Document;
		std::uint64_t Revision{};
		std::vector<FSceneHandle> Handles;
		FSceneHandle Source;
		FVec2 Start;
		FVec4 Bounds;
		bool bToggle{};
		bool bDragging{};
		bool bTargetPreview{};
	};

	std::optional<FReparentGesture> ReparentGesture;
	std::optional<std::optional<FSceneHandle>> ReparentDrop;
	std::unordered_set<std::string> ReparentOpenNodes;
	std::uint64_t ReparentSerial{};
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
	std::vector<std::byte> ContentSaveOriginal;
	std::map<std::string, FVec4> ContentTileBounds;
	FVec4 ContentClickBounds;
	const std::string& CurrentPath = SceneDocument.GetState().Path;
	std::string Error;
	std::string Filter;
	std::string SceneFilter;
	std::string CatalogError;
	bool bShowViewport = true;
	bool bShowOutliner = true;
	bool bShowDetails = true;
	std::unique_ptr<FAssetImportPanel> ImportPanel;
	std::uint64_t ImportRevision{};
	FVec4 ImportMenuBounds{};
	bool bImportVerified{};
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

	FRenderBenchmarkSample BenchmarkFrame;
	std::vector<FRenderBenchmarkSample> BenchmarkSamples;
	bool bBenchmarkTiming{};
	bool bBenchmarkLightEdit{};
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
	std::uint32_t LightPriorityExerciseStep{};
	std::uint32_t DepthExerciseStep{};
	FMat4 DepthExerciseFrozenView;
	FSceneCameraView DepthExerciseCamera;
	bool ExerciseLiveDepth(std::vector<FInputEvent>& InEvents);
	std::uint64_t TransformDragReadyAt{};
	FMat4 ExerciseTransformResult;
	bool bDocumentVerified{};
	bool bViewsVerified{};
	FSceneCameraView ExerciseInitialView;
	FSceneCameraView ExerciseEditorView;
};
} // namespace Hyperion
