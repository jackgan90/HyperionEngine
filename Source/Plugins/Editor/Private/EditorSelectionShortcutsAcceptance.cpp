#include "EditorApplication.h"
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

void ShortcutKey(std::vector<FInputEvent>& InEvents, EKey InKey, unsigned InModifiers = 0, bool bInDown = true,
                 bool bInRepeat = false)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.Modifiers = InModifiers;
	Event.bDown = bInDown;
	Event.bRepeat = bInRepeat;
	InEvents.push_back(Event);
}

void ShortcutMouse(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInDown, unsigned InButton = 0)
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

void FEditorPlugin::PrepareSelectionShortcuts()
{
	for (const auto Handle : Scene->GetNodes(ESceneNodeKind::Model))
	{
		Scene->SetModelVisible(Handle, false);
	}
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		FSceneNode Node;
		Node.Name = std::string("Shortcut ") + char('A' + Index);
		if (Index == 0 || Index == 2)
		{
			Node.Model() = FSceneModelComponent{};
			Node.Model()->Asset = PlacementModels.at("Cube").Asset;
			Node.Local() = Translation({Index ? 2.f : -2.f, 0, 0});
		}
		if (Index == 2)
		{
			Node.Parent() = Scene->FindNode(ShortcutObjects[1])->Id;
		}
		ShortcutObjects.push_back(Scene->AddNode(std::move(Node)));
	}
	FSceneNode Parent;
	Parent.Name = "Hidden parent";
	Parent.bEnabled = false;
	const auto ParentHandle = Scene->AddNode(std::move(Parent));
	for (unsigned Index = 0; Index < 130; ++Index)
	{
		FSceneNode Node;
		Node.Name = "Hidden child " + std::to_string(Index);
		Node.Parent() = Scene->FindNode(ParentHandle)->Id;
		Scene->AddNode(std::move(Node));
	}
	Filter = "Shortcut";
	bShowLightMarkers = false;
	ViewCamera.World = SceneCameraTransform({0, 0, 12}, {});
	bViewportCameraInitialized = true;
	ResetDocument();
	SelectObject(std::nullopt);
	ShortcutRevision = Scene->GetRevision();
	Gui->FocusWindow("Outliner");
}

void FEditorPlugin::CheckShortcutSelection(std::initializer_list<unsigned> InIndices)
{
	RequireShortcut(Selection.All().size() == InIndices.size(),
	                "selection size at step " + std::to_string(ShortcutStep) + ": expected " +
	                    std::to_string(InIndices.size()) + ", got " + std::to_string(Selection.All().size()));
	for (const auto Index : InIndices)
	{
		RequireShortcut(Selection.Contains(ShortcutObjects[Index]), "missing range member");
	}
	RequireShortcut(Selection.Primary() == ShortcutObjects[*(InIndices.end() - 1)], "range endpoint is not primary");
	RequireShortcut(Scene->GetRevision() == ShortcutRevision && !IsDirty() && History.empty(),
	                "selection mutated authored state");
}

void FEditorPlugin::ExerciseSelectionRanges(std::vector<FInputEvent>& InEvents)
{
	const unsigned Case = (ShortcutStep - 1) / 3;
	const unsigned Phase = (ShortcutStep - 1) % 3;
	const std::array<unsigned, 7> Targets{0, 3, 2, 3, 1, 0, 0};
	const std::array<unsigned, 7> Modifiers{0, 2, 2, 1, 2, 3, 2};
	const auto Bounds = MultiSelectionRows.at(Scene->FindNode(ShortcutObjects[Targets[Case]])->Id);
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
			Filter = "Shortcut A";
			break;
		case 6:
			CheckShortcutSelection({0});
			break;
	}
}

void FEditorPlugin::ExerciseSelectionKeys(std::vector<FInputEvent>& InEvents)
{
	// Stay inside the model face and clear of the primary object's gizmo handles.
	const auto Point = ProjectViewportPoint(ViewCamera, ViewportRegion.Bounds, {2.35f, -.35f, 0});
	RequireShortcut(bool(Point), "viewport point unavailable");
	const FVec2 Hit{Point->X, Point->Y};
	const FVec2 Miss{ViewportRegion.Bounds.X + 8, ViewportRegion.Bounds.Y + 8};
	switch (ShortcutStep)
	{
		case 22:
			Filter = "Shortcut";
			Gui->FocusWindow("Outliner");
			break;
		case 23:
			ShortcutKey(InEvents, EKey::A, 1);
			break;
		case 24:
		case 26:
		{
			RequireShortcut(Selection.All().size() == Scene->GetNodes().size() && Selection.All().size() > 128,
			                "Ctrl+A omitted hidden or search-excluded nodes");
			RequireShortcut(Selection.Primary() == ShortcutObjects[ShortcutStep == 24 ? 0 : 2], "Ctrl+A moved primary");
			const auto Before = Selection;
			const auto Summary = SelectAllSceneNodes(SceneDocument, {SceneDocument.Id(), ShortcutRevision});
			RequireShortcut(Selection == Before && Summary.Count == Before.All().size(),
			                "GUI/domain all-selection mismatch");
			SelectObject(ShortcutObjects[ShortcutStep == 24 ? 2 : 0]);
			Gui->FocusWindow("Viewport");
			break;
		}
		case 25:
			ShortcutKey(InEvents, EKey::A, 1);
			break;
		case 27:
		case 29:
		case 31:
			if (ShortcutStep == 29)
			{
				CheckShortcutSelection({0, 2});
			}
			if (ShortcutStep == 31)
			{
				CheckShortcutSelection({0});
			}
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(InEvents, ShortcutStep == 31 ? Miss : Hit, true);
			break;
		case 28:
		case 30:
		case 32:
			ShortcutMouse(InEvents, ShortcutStep == 32 ? Miss : Hit, false);
			break;
		case 33:
			CheckShortcutSelection({0});
			break;
	}
}

void FEditorPlugin::ExerciseSelectionGuards(std::vector<FInputEvent>& InEvents)
{
	const auto Search = ShortcutPoint(InspectionBounds.at("clipboard/search"));
	const FVec2 Miss{ViewportRegion.Bounds.X + 8, ViewportRegion.Bounds.Y + 8};
	switch (ShortcutStep)
	{
		case 34:
			ShortcutMouse(InEvents, Search, true);
			break;
		case 35:
			ShortcutMouse(InEvents, Search, false);
			break;
		case 36:
			ShortcutKey(InEvents, EKey::A, 1);
			break;
		case 37:
			CheckShortcutSelection({0});
			Gui->FinishEditing();
			Gui->FocusWindow("Details");
			break;
		case 38:
			ShortcutKey(InEvents, EKey::A, 1);
			break;
		case 39:
			CheckShortcutSelection({0});
			Gui->FocusWindow("Viewport");
			break;
		case 40:
			ShortcutMouse(InEvents, Miss, true, 1);
			ShortcutKey(InEvents, EKey::A, 1);
			break;
		case 41:
			CheckShortcutSelection({0});
			// The key belongs to navigation even if its later release shares the same event batch.
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutMouse(InEvents, Miss, false, 1);
			break;
		case 42:
			CheckShortcutSelection({0});
			ShortcutKey(InEvents, EKey::A, 1, true, true);
			break;
		case 43:
			CheckShortcutSelection({0});
			bRequestOpen = bOpenDialog = true;
			break;
		case 44:
			ShortcutKey(InEvents, EKey::A, 1);
			break;
		case 45:
			CheckShortcutSelection({0});
			bOpenDialog = false;
			Gui->ClosePopups();
			Gui->FocusWindow("Outliner");
			Filter.clear();
			break;
	}
}

void FEditorPlugin::ExerciseSelectionTree(std::vector<FInputEvent>& InEvents)
{
	const FVec2 Miss{ViewportRegion.Bounds.X + 8, ViewportRegion.Bounds.Y + 8};
	switch (ShortcutStep)
	{
		case 46:
		case 47:
		{
			const auto Bounds = MultiSelectionRows.at(Scene->FindNode(ShortcutObjects[1])->Id);
			ShortcutMouse(InEvents, {Bounds.X + Gui->Scale(8), (Bounds.Y + Bounds.W) / 2}, ShortcutStep == 46);
			break;
		}
		case 48:
			RequireShortcut(std::find(OutlinerRows.begin(), OutlinerRows.end(), ShortcutObjects[2]) ==
			                    OutlinerRows.end(),
			                "tree child did not fold");
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(InEvents, ShortcutPoint(MultiSelectionRows.at(Scene->FindNode(ShortcutObjects[3])->Id)),
			              true);
			break;
		case 49:
			ShortcutMouse(InEvents, ShortcutPoint(MultiSelectionRows.at(Scene->FindNode(ShortcutObjects[3])->Id)),
			              false);
			break;
		case 50:
			CheckShortcutSelection({0, 1, 3});
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(InEvents, ShortcutPoint(MultiSelectionRows.at(Scene->FindNode(ShortcutObjects[3])->Id)),
			              true);
			break;
		case 51:
			ShortcutMouse(InEvents, Miss, true);
			break;
		case 52:
			RequireShortcut(ReparentGesture && ReparentGesture->bDragging,
			                "Shift gesture did not enter drag arbitration");
			ShortcutKey(InEvents, EKey::Escape);
			break;
		case 53:
			ShortcutMouse(InEvents, Miss, false);
			ShortcutKey(InEvents, EKey::Escape, 0, false);
			break;
		case 54:
			CheckShortcutSelection({0, 1, 3});
			RequireShortcut(!ReparentGesture && Scene->FindNode(ShortcutObjects[2])->Parent() ==
			                                        Scene->FindNode(ShortcutObjects[1])->Id,
			                "cancelled Shift gesture reparented nodes");
			bSelectionShortcutsVerified = true;
			break;
	}
}

void FEditorPlugin::ExerciseSelectionShortcuts(std::vector<FInputEvent>& InEvents)
{
	if (!Scene->GetStatus().bReady || !bViewportVisible || bSelectionShortcutsVerified)
	{
		return;
	}
	if (++ShortcutWait % 3 != 0)
	{
		if (ShortcutWait % 3 == 1)
		{
			ShortcutKey(InEvents, EKey::A, 0, false);
		}
		return;
	}
	if (!ShortcutStep)
	{
		PrepareSelectionShortcuts();
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
	}
	else if (ShortcutStep <= 21)
	{
		ExerciseSelectionRanges(InEvents);
	}
	else if (ShortcutStep <= 33)
	{
		ExerciseSelectionKeys(InEvents);
	}
	else if (ShortcutStep <= 45)
	{
		ExerciseSelectionGuards(InEvents);
	}
	else
	{
		ExerciseSelectionTree(InEvents);
	}
	++ShortcutStep;
}
} // namespace Hyperion
