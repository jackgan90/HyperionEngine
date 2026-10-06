#include "EditorAcceptanceHarness.h"

namespace Hyperion
{
namespace
{
void CheckDeletion(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Deletion acceptance: ") + InMessage);
	}
}
} // namespace

bool FEditorAcceptanceHarness::ExerciseDeletionInput(std::vector<FInputEvent>& InEvents)
{
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Key = EKey::Delete;
	Key.bDown = true;
	switch (Scenario.Deletion.GetState())
	{
		case EDeletionState::RepeatDelete:
			CheckDeletion(bool(Editor.Selection), "click did not select deletion target");
			Scenario.DeletionExerciseHandle = *Editor.Selection;
			Scenario.DeletionExerciseId = Editor.Scene->FindNode(*Editor.Selection)->Id;
			Key.bRepeat = true;
			InEvents.push_back(Key);
			Scenario.Deletion.TransitionTo(EDeletionState::DeleteSelection);
			break;
		case EDeletionState::DeleteSelection:
			CheckDeletion(Editor.Selection == Scenario.DeletionExerciseHandle && !Editor.IsDirty(),
			              "repeat deleted an object");
			InEvents.push_back(Key);
			Scenario.Deletion.TransitionTo(EDeletionState::VerifyDeletion);
			break;
		case EDeletionState::VerifyDeletion:
			CheckDeletion(!Editor.Selection && !Editor.Scene->FindNode(Scenario.DeletionExerciseHandle) &&
			                  Editor.IsDirty(),
			              "Del did not delete and clear selection");
			CheckDeletion(Editor.HistoryCursor == 1, "delete was not one history entry");
			Scenario.Deletion.TransitionTo(EDeletionState::UndoDeletion);
			break;
		case EDeletionState::UndoDeletion:
			CheckDeletion(!Editor.Selection, "Outliner automatically selected a replacement");
			Editor.Undo();
			CheckDeletion(!Editor.IsDirty() && Editor.Scene->FindHandle(Scenario.DeletionExerciseId).Scene &&
			                  Editor.Selection == Editor.Scene->FindHandle(Scenario.DeletionExerciseId),
			              "undo did not restore the object, selection and save point");
			Editor.Redo();
			Scenario.Deletion.TransitionTo(EDeletionState::VerifyRedoAndRestore);
			break;
		case EDeletionState::VerifyRedoAndRestore:
			CheckDeletion(!Editor.Selection && !Editor.Scene->FindHandle(Scenario.DeletionExerciseId).Scene &&
			                  Editor.IsDirty(),
			              "redo did not delete and clear selection");
			Editor.Undo();
			Editor.SelectObject(Editor.Scene->FindHandle(Scenario.DeletionExerciseId));
			Editor.ResetDocument();
			Scenario.Deletion.TransitionTo(EDeletionState::RepeatDelete);
			return true;
	}
	return false;
}

void FEditorAcceptanceHarness::ExerciseDeletionHistory()
{
	const auto PreviousSelection = Editor.Selection;
	const auto Settings = Editor.Scene->GetSettings();
	FSceneNode Parent;
	Parent.Name = "Deletion parent";
	const auto ParentHandle = Editor.CommitCreate(Parent);
	const auto ParentId = Editor.Scene->FindNode(ParentHandle)->Id;
	auto Child = MakeSceneCameraNode({}, {0, 0, 10}, {});
	Child.Parent() = ParentId;
	const auto ChildHandle = Editor.CommitCreate(Child);
	const auto ChildId = Editor.Scene->FindNode(ChildHandle)->Id;
	auto Candidate = *Editor.Scene->FindNode(ChildHandle);
	Candidate.Name = "Edited child";
	Editor.CommitEdit(ChildHandle, Candidate, Editor.Scene->GetRevision());
	auto CameraSettings = Settings;
	CameraSettings.DefaultCamera = ChildHandle;
	Editor.CommitSettings(CameraSettings);
	Editor.SetPreviewCamera(ChildHandle);
	Editor.SelectObject(ParentHandle);
	Editor.CommitDelete();
	CheckDeletion(!Editor.Selection && !Editor.Viewport.PreviewCamera && !Editor.Scene->FindHandle(ChildId).Scene &&
	                  !Editor.Scene->GetSettings().DefaultCamera,
	              "subtree deletion left selection, preview or scene references");
	for (unsigned Cycle = 0; Cycle < 2; ++Cycle)
	{
		Editor.Undo();
		const auto RestoredChild = Editor.Scene->FindHandle(ChildId);
		CheckDeletion(Editor.Selection == Editor.Scene->FindHandle(ParentId) && RestoredChild != ChildHandle &&
		                  Editor.Scene->FindNode(RestoredChild)->Parent() == ParentId &&
		                  *Editor.Scene->FindNode(RestoredChild) == Candidate &&
		                  Editor.Scene->GetSettings().DefaultCamera == RestoredChild,
		              "subtree undo lost components, hierarchy or settings");
		Editor.SelectObject(RestoredChild);
		Editor.Redo();
		CheckDeletion(!Editor.Selection && !Editor.Scene->FindHandle(ParentId).Scene,
		              "subtree redo retained selection");
	}
	Editor.Undo();
	Editor.Undo();
	Editor.Undo();
	CheckDeletion(Editor.Scene->FindNode(Editor.Scene->FindHandle(ChildId))->Name != Candidate.Name,
	              "earlier child edit retained a stale handle");
	Editor.Undo();
	Editor.Undo();
	CheckDeletion(!Editor.Scene->FindHandle(ParentId).Scene && !Editor.IsDirty(),
	              "create undo failed after delete undo");
	Editor.Redo();
	Editor.Redo();
	Editor.Redo();
	Editor.Redo();
	Editor.Redo();
	CheckDeletion(!Editor.Selection && !Editor.Scene->FindHandle(ChildId).Scene,
	              "mixed create/edit/delete redo failed");
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		Editor.Undo();
	}
	CheckDeletion(Editor.Scene->GetSettings() == Settings && !Editor.IsDirty(),
	              "history lost original settings/save point");
	Editor.ResetDocument();
	Editor.SetSelection(PreviousSelection);
}
} // namespace Hyperion
