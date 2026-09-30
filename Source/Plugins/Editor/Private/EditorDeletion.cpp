#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
void FEditorPlugin::RouteDeleteShortcut(std::span<const FInputEvent> InEvents)
{
	if (!CaptureShortcutInteraction(InEvents).Allows(EEditorShortcut::Delete))
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
			UpdateDocumentInteraction();
			SceneDocument.RequireIdle(SceneDocument.Id(), Scene->GetRevision());
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
