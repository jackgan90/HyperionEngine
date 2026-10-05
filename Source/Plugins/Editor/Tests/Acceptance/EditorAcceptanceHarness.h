#pragma once
#include "../../Private/EditorApplication.h"
#include "EditorAcceptanceState.h"

namespace Hyperion
{
enum class EEditorAssetAcceptancePhase
{
	MainWindow,
	NameAndSave,
	AssetProperties,
	AssetWindowLifecycle
};

enum class EEditorAcceptanceWindow
{
	Main,
	Asset
};

class FEditorAcceptanceHarness
{
public:
	static std::unique_ptr<FEditorAcceptanceHarness> Create(FEditorPlugin& InEditor);
	explicit FEditorAcceptanceHarness(FEditorPlugin& InEditor);
	void CollectInput(std::vector<FInputEvent>& InEvents, std::vector<FInputEvent>& InAssetEvents);
	bool IsComplete() const;
	bool ShouldCapture() const;
	void CheckCompletion() const;
	void CheckTimeout(double InElapsed) const;
	void CheckGui(const FGuiDrawData& InData);
	void CheckAssetWindowFrame();
	void DrawPanels() const;
	void BeginSurface(EEditorSurface InSurface);
	void ObserveWidget(EEditorWidget InWidget, FVec4 InBounds, std::string_view InId);
	void ObserveIndexedWidget(EEditorWidget InWidget, FVec4 InBounds, std::size_t InIndex);
	std::function<void(std::string_view, FVec4)> PropertyObserver(std::string_view InComponent, bool bInSelection);
	void ObservePlacement(const std::shared_ptr<const FTransientGeometry>& InPreview) const;
	std::span<const FSceneHandle> OutlineSelection(std::span<const FSceneHandle> InSelection) const;
	std::filesystem::path MainCapture() const;
	std::filesystem::path TakeAssetCapture();
	void CompleteCapture(const FImage& InImage, const std::filesystem::path& InPath);
	bool RevealContent(std::string_view InPath);
	void WriteReport(std::ostream& InStream) const;
	const FEditorAcceptancePolicy& Policy() const;

private:
	void ObserveIdentifiedWidget(EEditorWidget InWidget, FVec4 InBounds, std::string_view InId);
	EEditorAssetAcceptancePhase AssetPhase() const;
	EEditorAcceptanceWindow AssetInputWindow() const;
	void BeginAssetRasterOptions();
	void CheckAssetRasterOptions() const;
	void CheckAssetSaveShortcut();
	void CheckFramingResult(FVec3 InCenter);
	void CheckGizmoHistory(unsigned InPhase);
	void CheckMultiSelectionMarkerDraws(const FGuiDrawData& InData);
	void CheckPendingAssetEdit();
	void CheckPlacementMarkerDraws(const FGuiDrawData& InData) const;
	void CheckRasterOptionFrame() const;
	void CheckViewportChoice() const;
	void CheckRenderControlsHud() const;
	void CheckShortcutSelection(std::initializer_list<unsigned> InIndices);
	void CompleteModelDrag(std::vector<FInputEvent>& InEvents, FVec2 InSource);
	void ExerciseAssetDiscardInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetNameInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetOpening(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetPanelInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseAssetPreviewHistory(std::vector<FInputEvent>& InEvents);
	bool ExerciseAssetPreviewInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetPropertyInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetReferences(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetSaving(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetTabClose(std::vector<FInputEvent>& InEvents, const std::string& InPath);
	void ExerciseAssetTextureInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowClosing(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowFixture(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowInput(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowSaving(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWindowSizing(std::vector<FInputEvent>& InEvents);
	void ExerciseAssetWorkspaceInput(std::vector<FInputEvent>& InEvents);
	void ExerciseCamera(std::vector<FInputEvent>& InEvents);
	void ExerciseCaptureHudInput(std::vector<FInputEvent>& InEvents);
	void ExerciseCaptureInput(std::vector<FInputEvent>& InEvents);
	void ExerciseClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds);
	void ExerciseClipboard(std::vector<FInputEvent>& InEvents);
	void ExerciseClipboardHistory(std::vector<FInputEvent>& InEvents);
	void ExerciseClipboardText(std::vector<FInputEvent>& InEvents);
	void ExerciseContentBrowser(std::vector<FInputEvent>& InEvents);
	void ExerciseContentClose(std::vector<FInputEvent>& InEvents);
	void ExerciseContentDismissal(std::vector<FInputEvent>& InEvents);
	void ExerciseContentFailures(std::vector<FInputEvent>& InEvents);
	void ExerciseContentInput(std::vector<FInputEvent>& InEvents);
	void ExerciseContentSaveAs(std::vector<FInputEvent>& InEvents);
	void ExerciseContentSwitch(std::vector<FInputEvent>& InEvents);
	void ExerciseCustomMaterialInput(std::vector<FInputEvent>& InEvents);
	void ExerciseCustomMaterialReset(std::vector<FInputEvent>& InEvents);
	void ExerciseDeletionHistory();
	bool ExerciseDeletionInput(std::vector<FInputEvent>& InEvents);
	void ExerciseDocumentInput(std::vector<FInputEvent>& InEvents);
	void ExerciseFraming(std::vector<FInputEvent>& InEvents);
	void ExerciseFramingGuards(std::vector<FInputEvent>& InEvents);
	void ExerciseFramingPopup(std::vector<FInputEvent>& InEvents);
	void ExerciseFramingSelection(std::vector<FInputEvent>& InEvents);
	void ExerciseFramingViewGuards(std::vector<FInputEvent>& InEvents);
	void ExerciseGizmoFocus(unsigned InPhase, FVec2 InStart, FVec2 InEnd, std::vector<FInputEvent>& InEvents);
	void ExerciseGizmoInput(std::vector<FInputEvent>& InEvents);
	void ExerciseImportInput(std::vector<FInputEvent>& InEvents);
	void ExerciseInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseLightPriorityInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseLiveDepth(std::vector<FInputEvent>& InEvents);
	void ExerciseLogInput(std::vector<FInputEvent>& InEvents);
	void ExerciseModelDrag(std::vector<FInputEvent>& InEvents);
	void ExerciseModelPlacement(std::vector<FInputEvent>& InEvents);
	void ExerciseModelPlacementHistory();
	void ExerciseMovement(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX, float InY);
	void ExerciseMultiCancellation();
	void ExerciseMultiDetails(std::vector<FInputEvent>& InEvents);
	void ExerciseMultiGizmo(std::vector<FInputEvent>& InEvents);
	void ExerciseMultiHistory();
	void ExerciseMultiSelection(std::vector<FInputEvent>& InEvents);
	void ExerciseMultiSelectionClicks(std::vector<FInputEvent>& InEvents);
	void ExerciseOutlines();
	void ExercisePickingInput(std::vector<FInputEvent>& InEvents);
	bool ExercisePickingScene(std::vector<FInputEvent>& InEvents);
	void ExercisePickingSelection(std::vector<FInputEvent>& InEvents, FVec2 InCenter, FVec2 InEmpty);
	void ExercisePickingView(std::vector<FInputEvent>& InEvents, FVec2 InCenter);
	void ExercisePlacedClipboard(std::vector<FInputEvent>& InEvents);
	void ExercisePlacedSkyActivation();
	void ExercisePlacementCancel(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementDocument(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementDrag(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementHistory();
	void ExercisePlacementInput(std::vector<FInputEvent>& InEvents);
	void ExercisePlacementMarkers(std::vector<FInputEvent>& InEvents);
	bool ExercisePlacementMenu(std::vector<FInputEvent>& InEvents);
	void ExerciseProfilingDetailsInput(std::vector<FInputEvent>& InEvents);
	void ExerciseProfilingHudInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseRasterOptions(std::vector<FInputEvent>& InEvents);
	bool ExerciseViewportChoices(std::vector<FInputEvent>& InEvents);
	void ExerciseRenderControlsInput(std::vector<FInputEvent>& InEvents);
	void ExerciseReparent(std::vector<FInputEvent>& InEvents);
	void ExerciseReparentReplacement(std::vector<FInputEvent>& InEvents);
	void ExerciseReparentDrag(std::vector<FInputEvent>& InEvents);
	void ExerciseReparentInterruption(std::vector<FInputEvent>& InEvents);
	void ExerciseReparentKeyboard(std::vector<FInputEvent>& InEvents);
	void ExerciseReparentSelection(std::vector<FInputEvent>& InEvents);
	void ExerciseSelectionGuards(std::vector<FInputEvent>& InEvents);
	void ExerciseSelectionKeys(std::vector<FInputEvent>& InEvents);
	void ExerciseSelectionRanges(std::vector<FInputEvent>& InEvents);
	void ExerciseSelectionShortcuts(std::vector<FInputEvent>& InEvents);
	void ExerciseSelectionTree(std::vector<FInputEvent>& InEvents);
	void ExerciseShortcutFocus(std::vector<FInputEvent>& InEvents);
	void ExerciseShortcutHistory(std::vector<FInputEvent>& InEvents);
	void ExerciseShortcutPopup(std::vector<FInputEvent>& InEvents);
	bool ExerciseShortcutRouting(std::vector<FInputEvent>& InEvents);
	void ExerciseShortcutText(std::vector<FInputEvent>& InEvents);
	bool ExerciseTransformDrag(std::vector<FInputEvent>& InEvents);
	bool ExerciseTransformHistory(std::vector<FInputEvent>& InEvents);
	bool ExerciseTransformInput(std::vector<FInputEvent>& InEvents);
	bool ExerciseUnchangedHistory(std::vector<FInputEvent>& InEvents);
	void ExerciseViewHistory();
	void ExerciseViewInput(std::vector<FInputEvent>& InEvents);
	void ExerciseViewPreview(std::vector<FInputEvent>& InEvents);
	void ExerciseWheel(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX, float InY);
	void PrepareClipboardExercise();
	void PrepareFramingExercise();
	void PrepareMultiSelection();
	void PrepareMultiSelectionMarkers();
	void PrepareOutlineExercise();
	void PreparePickingExercise();
	void PrepareReparentExercise();
	void PrepareSelectionShortcuts();
	void VerifyReparentExercise();
	FEditorPlugin& Editor;
	FEditorAcceptanceState Scenario;
	FEditorAcceptancePolicy ExecutionPolicy;
};
} // namespace Hyperion
