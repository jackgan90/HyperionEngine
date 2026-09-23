#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
void FEditorPlugin::RouteDeleteShortcut(std::span<const FInputEvent> InEvents)
{
	if (!Selection || !Scene->GetStatus().bReady || Gui->IsEditingText() || Gui->DragPayload() ||
	    Placement.IsActive() || Gizmo.IsDragging() || bCameraDragging || bOpenDialog || bSaveDialog || bAssetMessage ||
	    PendingRoot || bDiscardDialog || bPreferencesDialog || Gui->PointerState().bCancel)
	{
		return;
	}
	const bool bDelete = std::any_of(InEvents.begin(), InEvents.end(),
	                                 [](const FInputEvent& InEvent)
	                                 {
		                                 return InEvent.Type == EEventType::Key && InEvent.Key == EKey::Delete &&
		                                        InEvent.bDown && !InEvent.bRepeat && !InEvent.Modifiers;
	                                 });
	if (bDelete)
	{
		try
		{
			CommitDelete();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
}

void FEditorPlugin::CommitDelete()
{
	FinishInspectorEdit();
	SceneDocument.CommitDelete();
	Error.clear();
}
} // namespace Hyperion
