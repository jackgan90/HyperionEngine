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

void FEditorPlugin::RouteHistoryShortcuts(std::vector<FInputEvent>& InEvents)
{
	const bool bAllowSave = !Placement.IsActive() && !Gui->DragPayload() && !IsAssetWindowBlocked() &&
	                        Scene->GetStatus().bReady && !CurrentPath.empty() && !PendingSave;
	std::erase_if(InEvents,
	              [&](const FInputEvent& InEvent)
	              {
		              if (InEvent.Type != EEventType::Key || !InEvent.bDown || InEvent.bRepeat ||
		                  !(InEvent.Modifiers & 1) || InEvent.Key != EKey::S)
		              {
			              return false;
		              }
		              if (bAllowSave)
		              {
			              try
			              {
				              SaveScene(CurrentPath);
			              }
			              catch (const std::exception& Failure)
			              {
				              Error = Failure.what();
			              }
		              }
		              return true;
	              });
	const bool bAllowHistory = !Placement.IsActive() && !Gui->DragPayload() && !bOpenDialog && !bSaveDialog &&
	                           !bDiscardDialog && !bAssetMessage && !PendingRoot && !bPreferencesDialog &&
	                           (!Gui->IsEditingText() || InspectorInteraction != 0);
	std::erase_if(InEvents,
	              [&](const FInputEvent& InEvent)
	              {
		              if (!bAllowHistory || InEvent.Type != EEventType::Key || !InEvent.bDown ||
		                  !(InEvent.Modifiers & 1) || (InEvent.Key != EKey::Z && InEvent.Key != EKey::Y))
		              {
			              return false;
		              }
		              try
		              {
			              if (InEvent.Key == EKey::Y || (InEvent.Modifiers & 2))
			              {
				              Redo();
			              }
			              else
			              {
				              Undo();
			              }
		              }
		              catch (const std::exception& Failure)
		              {
			              Error = Failure.what();
		              }
		              return true;
	              });
}

void FEditorPlugin::CommitSettings(FSceneSettings InSettings)
{
	FinishInspectorEdit();
	SceneDocument.CommitSettings(std::move(InSettings));
	Error.clear();
}

} // namespace Hyperion
