#include "EditorApplication.h"

namespace Hyperion
{
FEditorShortcutInteraction FEditorPlugin::CaptureShortcutInteraction(std::span<const FInputEvent> InEvents) const
{
	FEditorShortcutInteraction Result;
	Result.bSceneFocus = (Viewport.bViewportVisible && Gui->IsWindowFocused("Viewport")) ||
	                     (bShowOutliner && Gui->IsWindowFocused("Outliner"));
	Result.bDetailsFocus = Gui->IsWindowFocused("Details");
	Result.bText = Gui->IsTextInputOwnedThisFrame();
	Result.bPopup = Gui->HasOpenPopup();
	const auto Pointer = Gui->PointerState();
	Result.bGesture = ReparentGesture || Placement.IsActive() || Gui->DragPayload() || Gizmo.IsDragging() ||
	                  bGizmoUsedMouse || bPlacementUsedMouse || Viewport.bCameraDragging || Pointer.bRightDown ||
	                  Pointer.bDown || Pointer.bCancel;
	Result.bBusy = !Options.Benchmark.empty() || bOpenDialog || bSaveDialog || Transition.IsDecisionVisible() ||
	               bAssetMessage || Transition.HasPendingRoot() || bPreferencesDialog || bFinished ||
	               IsAuxiliaryWindowBlocked();
	Result.bInspector = InspectorInteraction != 0 || InspectorTransaction.has_value();
	Result.bReady = Scene->GetStatus().bReady;
	for (const auto& Event : InEvents)
	{
		Result.bFocusLost |= Event.Type == EEventType::Focus && !Event.bDown;
		Result.bOwnershipTransition |= Event.Type == EEventType::MouseButton || Event.Type == EEventType::Focus ||
		                               Event.Type == EEventType::Text ||
		                               (Event.Type == EEventType::Key && Event.Key == EKey::Tab);
		// Any navigation press in this batch owns input, even when released before routing.
		Result.bGesture |= Event.Type == EEventType::MouseButton && Event.Button == InputButtons::Right;
	}
	return Result;
}
} // namespace Hyperion
