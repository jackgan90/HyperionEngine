#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::RouteClipboardShortcuts(std::span<const FInputEvent> InEvents)
{
	if (!CaptureShortcutInteraction(InEvents).Allows(EEditorShortcut::Clipboard))
	{
		return;
	}
	for (const auto& Event : InEvents)
	{
		if (Event.Type != EEventType::Key || !Event.bDown || Event.bRepeat ||
		    Event.Modifiers != InputModifiers::Control || (Event.Key != EKey::C && Event.Key != EKey::V))
		{
			continue;
		}
		try
		{
			UpdateDocumentInteraction();
			if (Event.Key == EKey::C)
			{
				SceneDocument.CopySelection(SceneDocument.Id(), Scene->GetRevision());
			}
			else
			{
				SceneDocument.PasteClipboard(SceneDocument.Id(), Scene->GetRevision());
			}
			Error.clear();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
}
} // namespace Hyperion
