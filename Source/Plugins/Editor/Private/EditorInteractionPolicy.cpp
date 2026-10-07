#include "EditorInteractionPolicy.h"

namespace Hyperion
{
bool FEditorInteractionPolicy::HasSceneDialog() const
{
	return Facts.bOpenDialog || Facts.bSaveDialog || Facts.bDecisionVisible || Facts.bAssetMessage ||
	       Facts.bPendingRoot || Facts.bPendingScene;
}

bool FEditorInteractionPolicy::HasPointerDialog() const
{
	return HasSceneDialog() || Facts.bPreferencesDialog;
}

bool FEditorInteractionPolicy::AllowsCameraNavigation() const
{
	return !HasPointerDialog() && !Facts.bReparentPending && !Facts.bPlacement && !Facts.bPlacementUsedMouse &&
	       !Facts.bGizmoDragging && !Facts.bGizmoUsedMouse && !Facts.bPreviewCamera && Facts.bCameraInitialized &&
	       Facts.bViewportVisible && !Facts.bEditingText && Facts.bViewportFocused;
}

bool FEditorInteractionPolicy::AllowsViewportPicking() const
{
	return !HasPointerDialog() && !Facts.bDragPayload && !Facts.bPlacement && !Facts.bPlacementUsedMouse &&
	       Facts.bViewportVisible && Facts.bViewportFocused && Facts.bViewportHovered && !Facts.bEditingText &&
	       Facts.bPointerPositionValid && !Facts.bPointerCancel && !Facts.bPointerRightDown && !Facts.bCameraDragging &&
	       !Facts.bGizmoUsedMouse && !Facts.bGizmoDragging;
}

bool FEditorInteractionPolicy::AllowsPlacement() const
{
	return !HasPointerDialog() && Facts.bViewportVisible && !Facts.bViewOptionsOpen && !Facts.bPointerCancel &&
	       !Facts.bPointerRightDown && !Facts.bGizmoDragging;
}

bool FEditorInteractionPolicy::AllowsGizmoOverlay() const
{
	// Existing gizmo presentation is independent of text ownership and the preferences dialog.
	return !HasSceneDialog() && Facts.bCameraInitialized && !Facts.bPreviewCamera;
}

bool FEditorInteractionPolicy::AllowsGizmo() const
{
	return AllowsGizmoOverlay() && !Facts.bReparentDragging && !Facts.bPlacement && !Facts.bPlacementUsedMouse;
}

bool FEditorInteractionPolicy::AllowsReparentBegin() const
{
	return !HasPointerDialog() && Facts.bSceneReady && !Facts.bDragPayload && !Facts.bPointerRightDown &&
	       !Facts.bPlacement;
}

bool FEditorInteractionPolicy::AllowsReparentContinue() const
{
	return !HasPointerDialog() && Facts.bSceneReady && !Facts.bPointerCancel && !Facts.bPointerRightDown &&
	       Facts.bPointerPositionValid;
}

bool FEditorInteractionPolicy::IsDocumentBusy() const
{
	return HasPointerDialog() || Facts.bBenchmark || Facts.bFinished || Facts.bGizmoEdit ||
	       Facts.bInspectorInteraction || Facts.bPendingInspectorEdit || Facts.bReparentPending || Facts.bPlacement ||
	       Facts.bDragPayload || Facts.bEditingText;
}

bool FEditorInteractionPolicy::BlocksAuxiliaryWindows() const
{
	return HasPointerDialog() || Facts.bSavingClose;
}

bool FEditorInteractionPolicy::AllowsScenePanels() const
{
	return !Facts.bPendingRoot && !Facts.bPendingScene && !Facts.bBenchmark;
}

bool FEditorInteractionPolicy::AllowsCloseRequest() const
{
	// A pending close decision must remain actionable through the shared close service.
	return !Facts.bFinished && !Facts.bOpenDialog && !Facts.bSaveDialog && !Facts.bPendingRoot &&
	       !Facts.bPendingScene && !Facts.bPreferencesDialog;
}

bool FEditorInteractionPolicy::HasCloseInteraction() const
{
	return Facts.bGizmoEdit || Facts.bInspectorInteraction || Facts.bPendingInspectorEdit || Facts.bPlacement;
}

FEditorShortcutInteraction FEditorInteractionPolicy::Shortcuts() const
{
	FEditorShortcutInteraction Result;
	Result.bSceneFocus = Facts.bSceneFocus;
	Result.bDetailsFocus = Facts.bDetailsFocus;
	Result.bText = Facts.bTextInputOwned;
	Result.bPopup = Facts.bPopup;
	Result.bGesture = Facts.bReparentPending || Facts.bPlacement || Facts.bDragPayload || Facts.bGizmoDragging ||
	                  Facts.bGizmoUsedMouse || Facts.bPlacementUsedMouse || Facts.bCameraDragging ||
	                  Facts.bPointerRightDown || Facts.bPointerDown || Facts.bPointerCancel;
	Result.bBusy = BlocksAuxiliaryWindows() || Facts.bBenchmark || Facts.bFinished;
	Result.bInspector = Facts.bInspectorInteraction || Facts.bInspectorTransaction;
	Result.bReady = Facts.bSceneReady;
	return Result;
}
} // namespace Hyperion
