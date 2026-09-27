#include "EditorApplication.h"

namespace Hyperion
{
namespace
{
void RequireClipboard(bool bInValue, const std::string& InMessage)
{
	if (!bInValue)
	{
		throw std::runtime_error("Clipboard acceptance: " + InMessage);
	}
}

void ClipboardKey(std::vector<FInputEvent>& InEvents, EKey InKey, bool bInDown = true, bool bInRepeat = false)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.bDown = bInDown;
	Event.bRepeat = bInRepeat;
	Event.Modifiers = bInDown ? 1 : 0;
	InEvents.push_back(Event);
}

void ClipboardClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = (InBounds.X + InBounds.Z) / 2;
	Event.Y = (InBounds.Y + InBounds.W) / 2;
	InEvents.push_back(Event);
	Event.Type = EEventType::MouseButton;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}
} // namespace

void FEditorPlugin::PrepareClipboardExercise()
{
	FSceneNode Parent;
	Parent.Id = "clipboard-parent";
	Parent.Name = "Clipboard group";
	auto CameraNode = MakeSceneCameraNode("clipboard-camera");
	CameraNode.Name = "Clipboard camera";
	CameraNode.Parent() = Parent.Id;
	const auto Models = Scene->GetNodes(ESceneNodeKind::Model);
	RequireClipboard(!Models.empty(), "fixture needs a resolved model");
	auto Model = *Scene->FindNode(Models.front());
	Model.Id = "clipboard-model";
	Model.Name = "Clipboard model";
	Model.Parent() = Parent.Id;
	if (auto& Source = Model.Components.Slot<FSceneModelSource>(); Source)
	{
		Source->InstanceRoot = Parent.Id;
	}
	const auto Nodes = Scene->AddNodes({Parent, CameraNode, Model});
	ResetDocument();
	FSceneSelection Chosen(Nodes[0]);
	Chosen.Toggle(Nodes[1]);
	SetSelection(std::move(Chosen));
	ClipboardExerciseCount = Scene->GetNodes().size();
	Window->SetClipboard("initial ordinary text");
	Gui->FocusWindow("Outliner");
}

void FEditorPlugin::ExerciseClipboardHistory(std::vector<FInputEvent>& InEvents)
{
	switch (ClipboardExerciseStep)
	{
		case 1:
			ClipboardKey(InEvents, EKey::C, true, true);
			break;
		case 2:
			RequireClipboard(Window->TypedClipboard(SceneClipboardFormat).empty(), "repeat copied objects");
			ClipboardKey(InEvents, EKey::C);
			break;
		case 3:
			RequireClipboard(SceneDocument.ClipboardInfo().Nodes == 3 && !IsDirty() && History.empty(),
			                 "copy snapshot or history: " + Error);
			ClipboardKey(InEvents, EKey::V);
			break;
		case 4:
			RequireClipboard(Scene->GetNodes().size() == ClipboardExerciseCount + 3 && HistoryCursor == 1 &&
			                     Selection.All().size() == 2,
			                 "first paste batch: " + Error);
			RequireClipboard(Scene->FindNode(*Selection)->Name == "Clipboard camera (1)", "numbered name");
			ClipboardKey(InEvents, EKey::V, true, true);
			break;
		case 5:
			RequireClipboard(HistoryCursor == 1, "repeat pasted objects");
			ClipboardKey(InEvents, EKey::V);
			break;
		case 6:
			RequireClipboard(Scene->GetNodes().size() == ClipboardExerciseCount + 6 && HistoryCursor == 2,
			                 "second paste batch: " + Error);
			Undo();
			Window->SetClipboard("ordinary text");
			ClipboardKey(InEvents, EKey::V);
			break;
		case 7:
			RequireClipboard(Scene->GetNodes().size() == ClipboardExerciseCount + 3 && HistoryCursor == 1,
			                 "text replacement pasted stale objects");
			Redo();
			RequireClipboard(Scene->GetNodes().size() == ClipboardExerciseCount + 6 && HistoryCursor == 2,
			                 "redo read the clipboard");
			SceneDocument.CopySelection(SceneDocument.Id(), Scene->GetRevision());
			Gui->FocusWindow("Content Browser");
			break;
		case 8:
			ClipboardKey(InEvents, EKey::V);
			break;
		case 9:
			RequireClipboard(HistoryCursor == 2, "Content Browser pasted scene nodes");
			Gui->FocusWindow("Outliner");
			ClipboardClick(InEvents, InspectionBounds.at("clipboard/search"), true);
			break;
	}
}

void FEditorPlugin::ExerciseClipboardText(std::vector<FInputEvent>& InEvents)
{
	switch (ClipboardExerciseStep)
	{
		case 10:
			ClipboardClick(InEvents, InspectionBounds.at("clipboard/search"), false);
			break;
		case 11:
			RequireClipboard(Gui->IsEditingText(), "search did not own text input");
			ClipboardKey(InEvents, EKey::A);
			break;
		case 12:
			ClipboardKey(InEvents, EKey::V);
			break;
		case 13:
			RequireClipboard(HistoryCursor == 2 && Filter.find("Clipboard") != std::string::npos,
			                 "text paste did not remain in search");
			ClipboardKey(InEvents, EKey::A);
			break;
		case 14:
			ClipboardKey(InEvents, EKey::C);
			break;
		case 15:
			RequireClipboard(Window->TypedClipboard(SceneClipboardFormat).empty(), "GUI text copy retained token");
			Gui->FinishEditing();
			Gui->FocusWindow("Viewport");
			Filter.clear();
			break;
		case 16:
			ClipboardKey(InEvents, EKey::V);
			break;
		case 17:
			RequireClipboard(HistoryCursor == 2 && Scene->GetNodes().size() == ClipboardExerciseCount + 6,
			                 "viewport pasted old object snapshot");
			Error.clear();
			bClipboardVerified = true;
			break;
	}
}

void FEditorPlugin::ExerciseClipboard(std::vector<FInputEvent>& InEvents)
{
	if (!Scene->GetStatus().bReady || !bViewportVisible || bClipboardVerified)
	{
		return;
	}
	if (++ClipboardExerciseWait % 3 != 0)
	{
		if (ClipboardExerciseWait % 3 == 1)
		{
			for (const auto Key : {EKey::A, EKey::C, EKey::V})
			{
				ClipboardKey(InEvents, Key, false);
			}
		}
		return;
	}
	if (!ClipboardExerciseStep)
	{
		PrepareClipboardExercise();
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
	}
	else if (ClipboardExerciseStep < 10)
	{
		ExerciseClipboardHistory(InEvents);
	}
	else
	{
		ExerciseClipboardText(InEvents);
	}
	++ClipboardExerciseStep;
}
} // namespace Hyperion
