#include "EditorApplication.h"

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

bool FEditorPlugin::ExerciseDeletionInput(std::vector<FInputEvent>& InEvents)
{
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Key = EKey::Delete;
	Key.bDown = true;
	switch (DeletionExerciseStep++)
	{
		case 0:
			CheckDeletion(Selection.has_value(), "click did not select deletion target");
			DeletionExerciseHandle = *Selection;
			DeletionExerciseId = Scene->FindNode(*Selection)->Id;
			Key.bRepeat = true;
			InEvents.push_back(Key);
			break;
		case 1:
			CheckDeletion(Selection == DeletionExerciseHandle && !IsDirty(), "repeat deleted an object");
			InEvents.push_back(Key);
			break;
		case 2:
			CheckDeletion(!Selection && !Scene->FindNode(DeletionExerciseHandle) && IsDirty(),
			              "Del did not delete and clear selection");
			CheckDeletion(HistoryCursor == 1, "delete was not one history entry");
			break;
		case 3:
			CheckDeletion(!Selection, "Outliner automatically selected a replacement");
			Undo();
			CheckDeletion(!IsDirty() && Scene->FindHandle(DeletionExerciseId).Scene &&
			                  Selection == Scene->FindHandle(DeletionExerciseId),
			              "undo did not restore the object, selection and save point");
			Redo();
			break;
		case 4:
			CheckDeletion(!Selection && !Scene->FindHandle(DeletionExerciseId).Scene && IsDirty(),
			              "redo did not delete and clear selection");
			Undo();
			SelectObject(Scene->FindHandle(DeletionExerciseId));
			ResetDocument();
			DeletionExerciseStep = 0;
			return true;
	}
	return false;
}

void FEditorPlugin::ExerciseDeletionHistory()
{
	const auto PreviousSelection = Selection;
	const auto Settings = Scene->GetSettings();
	FSceneNode Parent;
	Parent.Name = "Deletion parent";
	const auto ParentHandle = CommitCreate(Parent);
	const auto ParentId = Scene->FindNode(ParentHandle)->Id;
	auto Child = MakeSceneCameraNode({}, {0, 0, 10}, {});
	Child.Parent() = ParentId;
	const auto ChildHandle = CommitCreate(Child);
	const auto ChildId = Scene->FindNode(ChildHandle)->Id;
	auto Candidate = *Scene->FindNode(ChildHandle);
	Candidate.Name = "Edited child";
	CommitEdit(ChildHandle, Candidate, Scene->GetRevision());
	auto CameraSettings = Settings;
	CameraSettings.DefaultCamera = ChildHandle;
	CommitSettings(CameraSettings);
	SetPreviewCamera(ChildHandle);
	SelectObject(ParentHandle);
	CommitDelete();
	CheckDeletion(!Selection && !PreviewCamera && !Scene->FindHandle(ChildId).Scene &&
	                  !Scene->GetSettings().DefaultCamera,
	              "subtree deletion left selection, preview or scene references");
	for (unsigned Cycle = 0; Cycle < 2; ++Cycle)
	{
		Undo();
		const auto RestoredChild = Scene->FindHandle(ChildId);
		CheckDeletion(Selection == Scene->FindHandle(ParentId) && RestoredChild != ChildHandle &&
		                  Scene->FindNode(RestoredChild)->Parent() == ParentId &&
		                  *Scene->FindNode(RestoredChild) == Candidate &&
		                  Scene->GetSettings().DefaultCamera == RestoredChild,
		              "subtree undo lost components, hierarchy or settings");
		SelectObject(RestoredChild);
		Redo();
		CheckDeletion(!Selection && !Scene->FindHandle(ParentId).Scene, "subtree redo retained selection");
	}
	Undo();
	Undo();
	Undo();
	CheckDeletion(Scene->FindNode(Scene->FindHandle(ChildId))->Name != Candidate.Name,
	              "earlier child edit retained a stale handle");
	Undo();
	Undo();
	CheckDeletion(!Scene->FindHandle(ParentId).Scene && !IsDirty(), "create undo failed after delete undo");
	Redo();
	Redo();
	Redo();
	Redo();
	Redo();
	CheckDeletion(!Selection && !Scene->FindHandle(ChildId).Scene, "mixed create/edit/delete redo failed");
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		Undo();
	}
	CheckDeletion(Scene->GetSettings() == Settings && !IsDirty(), "history lost original settings/save point");
	ResetDocument();
	SelectObject(PreviousSelection);
}
} // namespace Hyperion
