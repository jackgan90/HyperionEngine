#include "EditorApplication.h"

namespace Hyperion
{
FEditorInteractionPolicy FEditorPlugin::CaptureInteractionPolicy() const
{
	FEditorInteractionFacts Facts;
	Facts.bOpenDialog = bOpenDialog;
	Facts.bSaveDialog = bSaveDialog;
	Facts.bDecisionVisible = Transition.IsDecisionVisible();
	Facts.bAssetMessage = bAssetMessage;
	Facts.bPendingRoot = Transition.HasPendingRoot();
	Facts.bPreferencesDialog = bPreferencesDialog;
	Facts.bSavingClose = Transition.IsSavingClose();
	Facts.bBenchmark = !Options.Benchmark.empty();
	Facts.bFinished = bFinished;
	Facts.bReparentPending = ReparentGesture.has_value();
	Facts.bReparentDragging = ReparentGesture && ReparentGesture->bDragging;
	Facts.bPlacement = Placement.IsActive();
	Facts.bPlacementUsedMouse = bPlacementUsedMouse;
	Facts.bGizmoDragging = Gizmo.IsDragging();
	Facts.bGizmoUsedMouse = bGizmoUsedMouse;
	Facts.bGizmoEdit = GizmoEdit.has_value();
	Facts.bInspectorInteraction = InspectorInteraction != 0;
	Facts.bPendingInspectorEdit = PendingInspectorEdit.has_value();
	Facts.bInspectorTransaction = InspectorTransaction.has_value();
	Facts.bSceneReady = Scene && Scene->GetStatus().bReady;
	Facts.bPreviewCamera = Viewport.PreviewCamera.has_value();
	Facts.bCameraInitialized = Viewport.bViewportCameraInitialized;
	Facts.bViewportVisible = Viewport.bViewportVisible;
	Facts.bViewportFocused = Viewport.ViewportRegion.bFocused;
	Facts.bViewportHovered = Viewport.ViewportRegion.bHovered;
	Facts.bCameraDragging = Viewport.bCameraDragging;
	Facts.bViewOptionsOpen = bViewOptionsOpen;
	if (Gui)
	{
		Facts.bDragPayload = Gui->DragPayload().has_value();
		Facts.bEditingText = Gui->IsEditingText();
		Facts.bTextInputOwned = Gui->IsTextInputOwnedThisFrame();
		Facts.bPopup = Gui->HasOpenPopup();
		Facts.bSceneFocus = (Viewport.bViewportVisible && Gui->IsWindowFocused("Viewport")) ||
		                    (bShowOutliner && Gui->IsWindowFocused("Outliner"));
		Facts.bDetailsFocus = Gui->IsWindowFocused("Details");
		const auto Pointer = Gui->PointerState();
		Facts.bPointerRightDown = Pointer.bRightDown;
		Facts.bPointerDown = Pointer.bDown;
		Facts.bPointerCancel = Pointer.bCancel;
		Facts.bPointerPositionValid = Pointer.bPositionValid;
	}
	return FEditorInteractionPolicy(Facts);
}
} // namespace Hyperion
