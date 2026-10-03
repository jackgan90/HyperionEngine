#pragma once
#include "AssetEditorWindow.h"
#include "AssetImportPanel.h"
#include "AssetWorkspace.h"
#include "ContentBrowser.h"
#include "EditorAcceptanceDriver.h"
#include "EditorDocumentTransition.h"
#include "EditorInspectionCache.h"
#include "EditorInteraction.h"
#include "EditorOptions.h"
#include "EditorSelection.h"
#include "EditorShortcuts.h"
#include "EditorViewport.h"
#include "Hyperion/Config/ApplicationClose.h"
#include "Hyperion/Content/ContentRootService.h"
#include "Hyperion/Core/Logging/LogHistory.h"
#include "Hyperion/Core/ProfilingControl.h"
#include "Hyperion/Core/ProfilingSession.h"
#include "Hyperion/Renderer/RenderBenchmark.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneLightControls.h"
#include "OutlinerSelection.h"
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

namespace Hyperion
{

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
	void FrameSelection(const FSceneMutationRequest& InRequest) override;
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
	friend class FEditorAcceptanceHarness;
	FEditorAcceptanceDriver Acceptance;
	FRenderSettings Rendering;
	std::uint64_t RenderSettingsRevision = 1;
	bool bShowRenderSettings{};
	bool bShowStatusHud{};
	bool bShowProfilingHud{};
	std::uint32_t ProfilingCategories = 1;
	FRenderDiagnostics HudDiagnostics;
	std::uint64_t HudUpdated{};
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
	FVec4 ProfilingOptionsAnchor;
	double FrameIntervalMilliseconds{};
	void DrawDebugBounds();
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
	bool IsAuxiliaryWindowBlocked() const;
	void SynchronizeWindowGroup();
	void AdvanceAssetWindow(float InDelta, std::vector<FInputEvent> InEvents,
	                        const std::filesystem::path& InCapture = {});

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
	void PrepareContentRoot(const std::filesystem::path& InRequested);
	void CompleteContentRoot(const FEditorRootCommit& InRequest);
	FEditorSaveProgress DocumentSaveProgress() const;
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
	void DrawWindowMenu();
	void DrawLog();
	bool bShowLog{};
	bool bFocusLog{};
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
	FPlacementPreparation GetPlacementPreparation(const FPlaceableObject& InObject) const;
	FPlacementPreparation GetPlacementPreparation(const FPlacementCandidate& InCandidate) const;
	FPlacementPreparationContext GetPlacementPreparationContext(const FPlacementCandidate& InCandidate) const;
	std::optional<FSceneNodeInfo> PollPlacement(const FPlacementCandidate& InCandidate, FVec3 InPosition);
	FPlacementCandidate ResolvePlacementPayload(const FGuiDragPayload& InPayload);
	void RoutePlacementPayload(const FGuiDragPayload& InPayload, const std::optional<FGuiDragPayload>& InDrop);
	void RoutePlacement();
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
	std::string StatusText() const;
	FGuiDrawData DrawGui(float InDelta, std::span<const FInputEvent> InEvents);
	void RouteCamera(float InDelta, std::span<const FInputEvent> InEvents);
	void RouteFrameSelectionShortcut(std::span<const FInputEvent> InEvents);
	void RouteSelectAllShortcut(std::span<const FInputEvent> InEvents);
	void ClickOutlinerObject(FSceneHandle InHandle, bool bInToggle, bool bInRange);
	void DrawViewportOverlays();
	void Render(FGuiDrawData InGui, bool bInCapture);
	std::shared_ptr<FSelectionOutlineRequest> MakeSelectionOutline(
	    const std::shared_ptr<const FSceneFrameSeed>& InSeed) const;
	void CompleteFrameCapture(const FImage& InImage, bool bInCapture, bool bInAgentCapture,
	                          const std::filesystem::path& InAuxiliaryCapture);
	FImage ExecuteEditorGraph(FRenderGraph InGraph, FSize InSize, bool bInScreenshot, FNativeSurface InSurface,
	                          bool bInCaptureRdc, bool bInVsync);
	void ResizeViewport();
	void RouteViewportPicking(std::span<const FInputEvent> InEvents);
	std::optional<FSceneCameraView> PickingCamera() const;
	void SaveLayout();
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
	void RouteHistoryShortcuts(std::span<const FInputEvent> InEvents);
	FEditorShortcutInteraction CaptureShortcutInteraction(std::span<const FInputEvent> InEvents) const;
	void RouteClipboardShortcuts(std::span<const FInputEvent> InEvents);
	void RouteDeleteShortcut(std::span<const FInputEvent> InEvents);
	void CommitDelete();
	void Undo();
	void Redo();
	void SaveScene(const std::string& InDestination);
	void PollSave();
	void SaveBeforeClose();
	void PollSavedClose();
	void DrawSaveDialog();
	void DrawDiscardDialog();
	void SaveBeforeRootSwitch();
	void ConfirmDiscardAction();
	void BeginBenchmarkTiming();
	void FinishFrameTiming(std::uint64_t InStarted, bool bInSceneReady);
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
	std::unique_ptr<FWindowGroup> WindowGroup;
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
	FEditorViewport Viewport;
	FSceneCameraController& Camera = Viewport.Navigation;
	FEditorDocumentTransition Transition;
	bool bViewOptionsOpen{};
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
	FPlacementPreparation PlacementPreparation;

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
		FPlacementPreparation Preparation{EPlacementPreparationState::Pending, EPlacementPreparationStage::IconLoading};
	};

	std::map<std::string, FPlacementIcon> PlacementIcons;
	std::shared_ptr<const FRenderMaterial> PlacementMaterial;
	std::shared_ptr<const void> PlacementLifetime;
	std::optional<FSceneHandle> PlacementPublication;
	FMat4 PlacementPublicationLocal;
	std::uint64_t PlacementPublicationRevision{};
	bool bPlacementPublicationSourceMaterials{};
	std::shared_ptr<const FTransientGeometry> PlacementPublicationPreview;
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
	FForwardPipelineStatistics RenderStats;
	FDeviceStats DeviceStats;
	std::vector<std::string> ScenePaths;
	FEditorSelection& Selection = SceneDocument.Selection();
	FSelectionOutlineSettings OutlineSettings;
	bool bSelectionInitialized{};

	std::optional<FViewportClick> ViewportClick;

	std::optional<FReparentGesture> ReparentGesture;
	FOutlinerSelectionState OutlinerSelection;
	std::vector<FSceneHandle> OutlinerRows;
	std::string OutlinerSelectionFilter;
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
	bool bShowBrowser = true;
	bool bOpenDialog{};
	bool bRequestOpen{};
	bool bResetLayout{};
	bool bStopped{};
	bool bReadyLogged{};
	std::uint32_t FrameCount{};
	std::uint32_t ReadyFrames{};
	std::uint32_t OpenCount{};
	float Exposure = 2.5f;

	bool bLoadErrorObserved{};

	FRenderBenchmarkSample BenchmarkFrame;
	std::vector<FRenderBenchmarkSample> BenchmarkSamples;
	bool bBenchmarkTiming{};
	bool bBenchmarkLightEdit{};
	std::uint64_t BenchmarkStarted{};
	double LoadMilliseconds{};

	const std::vector<FSceneHistoryEntry>& History = SceneDocument.GetState().History;
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
};
} // namespace Hyperion
