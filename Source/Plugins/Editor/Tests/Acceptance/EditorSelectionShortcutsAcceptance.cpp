#include "EditorAcceptanceHarness.h"
#include "Hyperion/SceneEditing/SceneAuthoring.h"

namespace Hyperion
{
namespace
{
void RequireShortcut(bool bInCondition, const std::string& InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error("Selection shortcut acceptance: " + InMessage);
	}
}

FVec2 ShortcutPoint(FVec4 InBounds)
{
	return {(InBounds.X + InBounds.Z) / 2, (InBounds.Y + InBounds.W) / 2};
}

void ShortcutKey(std::vector<FInputEvent>& InEvents, EKey InKey, unsigned InModifiers = InputModifiers::None,
                 bool bInDown = true, bool bInRepeat = false)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.Modifiers = InModifiers;
	Event.bDown = bInDown;
	Event.bRepeat = bInRepeat;
	InEvents.push_back(Event);
}

void ShortcutMouse(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInDown,
                   unsigned InButton = InputButtons::Left)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
	Event.Type = EEventType::MouseButton;
	Event.Button = InButton;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}
} // namespace

void FEditorAcceptanceHarness::PrepareSelectionShortcuts()
{
	for (const auto Handle : Editor.Scene->GetNodes(ESceneNodeKind::Model))
	{
		Editor.Scene->SetModelVisible(Handle, false);
	}
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		FSceneNode Node;
		Node.Name = std::string("Shortcut ") + char('A' + Index);
		if (Index == 0 || Index == 2)
		{
			Node.Model() = FSceneModelComponent{};
			Node.Model()->Asset = Editor.PlacementModels.at("Cube").Asset;
			Node.Local() = Translation({Index ? 2.f : -2.f, 0, 0});
		}
		if (Index == 2)
		{
			Node.Parent() = Editor.Scene->FindNode(Scenario.ShortcutObjects[1])->Id;
		}
		Scenario.ShortcutObjects.push_back(Editor.Scene->AddNode(std::move(Node)));
	}
	FSceneNode Parent;
	Parent.Name = "Hidden parent";
	Parent.bEnabled = false;
	const auto ParentHandle = Editor.Scene->AddNode(std::move(Parent));
	for (unsigned Index = 0; Index < 130; ++Index)
	{
		FSceneNode Node;
		Node.Name = "Hidden child " + std::to_string(Index);
		Node.Parent() = Editor.Scene->FindNode(ParentHandle)->Id;
		Editor.Scene->AddNode(std::move(Node));
	}
	Editor.Filter = "Shortcut";
	Editor.bShowLightMarkers = false;
	Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 12}, {});
	Editor.Viewport.bViewportCameraInitialized = true;
	Editor.ResetDocument();
	Editor.SelectObject(std::nullopt);
	Scenario.ShortcutRevision = Editor.Scene->GetRevision();
	Editor.Gui->FocusWindow("Outliner");
}

void FEditorAcceptanceHarness::CheckShortcutSelection(std::initializer_list<unsigned> InIndices)
{
	RequireShortcut(Editor.Selection.All().size() == InIndices.size(),
	                "selection size at step " + std::to_string(Scenario.ShortcutStep) + ": expected " +
	                    std::to_string(InIndices.size()) + ", got " + std::to_string(Editor.Selection.All().size()));
	for (const auto Index : InIndices)
	{
		RequireShortcut(Editor.Selection.Contains(Scenario.ShortcutObjects[Index]), "missing range member");
	}
	RequireShortcut(Editor.Selection.Primary() == Scenario.ShortcutObjects[*(InIndices.end() - 1)],
	                "range endpoint is not primary");
	RequireShortcut(Editor.Scene->GetRevision() == Scenario.ShortcutRevision && !Editor.IsDirty() &&
	                    Editor.History.empty(),
	                "selection mutated authored state");
}

void FEditorAcceptanceHarness::ExerciseSelectionRanges(std::vector<FInputEvent>& InEvents)
{
	const unsigned Case = (Scenario.ShortcutStep - 1) / 3;
	const unsigned Phase = (Scenario.ShortcutStep - 1) % 3;
	const std::array<unsigned, 7> Targets{0, 3, 2, 3, 1, 0, 0};
	const std::array<unsigned, 7> Modifiers{0, 2, 2, 1, 2, 3, 2};
	const auto Bounds =
	    Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[Targets[Case]])->Id);
	if (Phase < 2)
	{
		ShortcutKey(InEvents, EKey::None, Phase == 0 ? Modifiers[Case] : 0);
		ShortcutMouse(InEvents, ShortcutPoint(Bounds), Phase == 0);
		return;
	}
	switch (Case)
	{
		case 0:
			CheckShortcutSelection({0});
			break;
		case 1:
			CheckShortcutSelection({0, 1, 2, 3});
			break;
		case 2:
			CheckShortcutSelection({0, 1, 2});
			break;
		case 3:
			CheckShortcutSelection({0, 1, 2, 3});
			break;
		case 4:
			CheckShortcutSelection({2, 3, 1});
			break;
		case 5:
			CheckShortcutSelection({1, 2, 3, 0});
			Editor.Filter = "Shortcut A";
			break;
		case 6:
			CheckShortcutSelection({0});
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSelectionKeys(std::vector<FInputEvent>& InEvents)
{
	// Stay inside the model face and clear of the primary object's gizmo handles.
	const auto Point =
	    ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds, {2.35f, -.35f, 0});
	RequireShortcut(bool(Point), "viewport point unavailable");
	const FVec2 Hit{Point->X, Point->Y};
	const FVec2 Miss{Editor.Viewport.ViewportRegion.Bounds.X + 8, Editor.Viewport.ViewportRegion.Bounds.Y + 8};
	switch (Scenario.ShortcutStep)
	{
		case 22:
			Editor.Filter = "Shortcut";
			Editor.Gui->FocusWindow("Outliner");
			break;
		case 23:
			ShortcutKey(InEvents, EKey::A, 1);
			break;
		case 24:
		case 26:
		{
			RequireShortcut(Editor.Selection.All().size() == Editor.Scene->GetNodes().size() &&
			                    Editor.Selection.All().size() > 128,
			                "Ctrl+A omitted hidden or search-excluded nodes");
			RequireShortcut(Editor.Selection.Primary() == Scenario.ShortcutObjects[Scenario.ShortcutStep == 24 ? 0 : 2],
			                "Ctrl+A moved primary");
			const auto Before = Editor.Selection;
			const auto Summary =
			    SelectAllSceneNodes(Editor.SceneDocument, {Editor.SceneDocument.Id(), Scenario.ShortcutRevision});
			RequireShortcut(Editor.Selection == Before && Summary.Count == Before.All().size(),
			                "GUI/domain all-selection mismatch");
			Editor.SelectObject(Scenario.ShortcutObjects[Scenario.ShortcutStep == 24 ? 2 : 0]);
			Editor.Gui->FocusWindow("Viewport");
			break;
		}
		case 25:
			ShortcutKey(InEvents, EKey::A, 1);
			break;
		case 27:
		case 29:
		case 31:
			if (Scenario.ShortcutStep == 29)
			{
				CheckShortcutSelection({0, 2});
			}
			if (Scenario.ShortcutStep == 31)
			{
				CheckShortcutSelection({0});
			}
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(InEvents, Scenario.ShortcutStep == 31 ? Miss : Hit, true);
			break;
		case 28:
		case 30:
		case 32:
			ShortcutMouse(InEvents, Scenario.ShortcutStep == 32 ? Miss : Hit, false);
			break;
		case 33:
			CheckShortcutSelection({0});
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSelectionGuards(std::vector<FInputEvent>& InEvents)
{
	const auto Search = ShortcutPoint(Scenario.InspectionBounds.at("clipboard/search"));
	const FVec2 Miss{Editor.Viewport.ViewportRegion.Bounds.X + 8, Editor.Viewport.ViewportRegion.Bounds.Y + 8};
	switch (Scenario.ShortcutStep)
	{
		case 34:
			ShortcutMouse(InEvents, Search, true);
			break;
		case 35:
			ShortcutMouse(InEvents, Search, false);
			break;
		case 36:
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutKey(InEvents, EKey::Delete);
			break;
		case 37:
			CheckShortcutSelection({0});
			Editor.Gui->FinishEditing();
			Editor.Gui->FocusWindow("Details");
			break;
		case 38:
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutKey(InEvents, EKey::Delete);
			break;
		case 39:
			CheckShortcutSelection({0});
			Editor.Gui->FocusWindow("Viewport");
			break;
		case 40:
			ShortcutMouse(InEvents, Miss, true, 1);
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutKey(InEvents, EKey::Delete);
			break;
		case 41:
			CheckShortcutSelection({0});
			// The key belongs to navigation even if its later release shares the same event batch.
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutMouse(InEvents, Miss, false, 1);
			ShortcutKey(InEvents, EKey::Delete);
			break;
		case 42:
			CheckShortcutSelection({0});
			ShortcutKey(InEvents, EKey::A, 1, true, true);
			ShortcutKey(InEvents, EKey::Delete, InputModifiers::None, true, true);
			break;
		case 43:
			CheckShortcutSelection({0});
			Editor.bRequestOpen = Editor.bOpenDialog = true;
			break;
		case 44:
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutKey(InEvents, EKey::Delete);
			break;
		case 45:
			CheckShortcutSelection({0});
			Editor.bOpenDialog = false;
			Editor.Gui->ClosePopups();
			Editor.Gui->FocusWindow("Outliner");
			Editor.Filter.clear();
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSelectionTree(std::vector<FInputEvent>& InEvents)
{
	const FVec2 Miss{Editor.Viewport.ViewportRegion.Bounds.X + 8, Editor.Viewport.ViewportRegion.Bounds.Y + 8};
	switch (Scenario.ShortcutStep)
	{
		case 46:
		case 47:
		{
			const auto Bounds = Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[1])->Id);
			ShortcutMouse(InEvents, {Bounds.X + Editor.Gui->Scale(8), (Bounds.Y + Bounds.W) / 2},
			              Scenario.ShortcutStep == 46);
			break;
		}
		case 48:
			RequireShortcut(std::find(Editor.OutlinerRows.begin(), Editor.OutlinerRows.end(),
			                          Scenario.ShortcutObjects[2]) == Editor.OutlinerRows.end(),
			                "tree child did not fold");
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(
			    InEvents,
			    ShortcutPoint(Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[3])->Id)),
			    true);
			break;
		case 49:
			ShortcutMouse(
			    InEvents,
			    ShortcutPoint(Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[3])->Id)),
			    false);
			break;
		case 50:
			CheckShortcutSelection({0, 1, 3});
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(
			    InEvents,
			    ShortcutPoint(Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[3])->Id)),
			    true);
			break;
		case 51:
			ShortcutMouse(InEvents, Miss, true);
			break;
		case 52:
			RequireShortcut(Editor.Reparent.GetGesture() && Editor.Reparent.GetGesture()->bDragging,
			                "Shift gesture did not enter drag arbitration");
			ShortcutKey(InEvents, EKey::Escape);
			ShortcutKey(InEvents, EKey::Delete);
			break;
		case 53:
			ShortcutMouse(InEvents, Miss, false);
			ShortcutKey(InEvents, EKey::Escape, 0, false);
			break;
		case 54:
			CheckShortcutSelection({0, 1, 3});
			RequireShortcut(!Editor.Reparent.GetGesture() &&
			                    Editor.Scene->FindNode(Scenario.ShortcutObjects[2])->Parent() ==
			                        Editor.Scene->FindNode(Scenario.ShortcutObjects[1])->Id,
			                "cancelled Shift gesture reparented nodes");
			Editor.SelectObject(Scenario.ShortcutObjects[0]);
			Editor.Gui->FocusWindow("Content Browser");
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSelectionShortcuts(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Scene->GetStatus().bReady || !Editor.Viewport.bViewportVisible || Scenario.bSelectionShortcutsVerified)
	{
		return;
	}
	if (++Scenario.ShortcutWait % 3 != 0)
	{
		if (Scenario.ShortcutWait % 3 == 1)
		{
			for (const auto Key : {EKey::A, EKey::Delete, EKey::Z, EKey::Y, EKey::S, EKey::Tab, EKey::End})
			{
				ShortcutKey(InEvents, Key, InputModifiers::None, false);
			}
		}
		return;
	}
	if (!Scenario.ShortcutStep)
	{
		PrepareSelectionShortcuts();
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
	}
	else if (Scenario.ShortcutStep <= 21)
	{
		ExerciseSelectionRanges(InEvents);
	}
	else if (Scenario.ShortcutStep <= 33)
	{
		ExerciseSelectionKeys(InEvents);
	}
	else if (Scenario.ShortcutStep <= 45)
	{
		ExerciseSelectionGuards(InEvents);
	}
	else if (Scenario.ShortcutStep <= 54)
	{
		ExerciseSelectionTree(InEvents);
	}
	else if (!ExerciseShortcutRouting(InEvents))
	{
		return;
	}
	++Scenario.ShortcutStep;
}
} // namespace Hyperion
