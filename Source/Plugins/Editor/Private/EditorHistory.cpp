#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
FSceneHandle FEditorPlugin::CommitCreate(FSceneNode InNode)
{
	FinishInspectorEdit();
	const auto Handle = SceneDocument.CommitCreate(std::move(InNode));
	bSelectionInitialized = true;
	Error.clear();
	return Handle;
}

void FEditorPlugin::FinishInspectorEdit()
{
	FinishGizmo();
	if (Gui && (InspectorInteraction || InspectorTransaction))
	{
		Gui->FinishEditing();
	}
	PendingInspectorEdit.reset();
	SceneDocument.FinishInteraction();
	SceneDocument.SetInteractionState(false, false);
	InspectorInteraction = 0;
}

void FEditorPlugin::RouteHistoryShortcuts(std::span<const FInputEvent> InEvents)
{
	const auto Interaction = CaptureShortcutInteraction(InEvents);
	for (const auto& Event : InEvents)
	{
		if (Event.Type != EEventType::Key || !Event.bDown ||
		    !HasInputModifier(Event.Modifiers, InputModifiers::Control))
		{
			continue;
		}
		try
		{
			if (Event.Key == EKey::S && !Event.bRepeat && Interaction.Allows(EEditorShortcut::Save) &&
			    !CurrentPath.empty() && !PendingSave)
			{
				SaveScene(CurrentPath);
			}
			else if ((Event.Key == EKey::Z || Event.Key == EKey::Y) && Interaction.Allows(EEditorShortcut::History))
			{
				// Runs after NewFrame but before widgets: FinishEditing retires the inspector's native text owner.
				// Ownership-changing batches are declined, so ordinary text Undo remains entirely native.
				if (Event.Key == EKey::Y || HasInputModifier(Event.Modifiers, InputModifiers::Shift))
				{
					Redo();
				}
				else
				{
					Undo();
				}
			}
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
}

void FEditorPlugin::CommitSettings(FSceneSettings InSettings)
{
	FinishInspectorEdit();
	SceneDocument.CommitSettings(std::move(InSettings));
	Error.clear();
}

} // namespace Hyperion
