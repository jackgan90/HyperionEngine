#include "EditorAcceptanceHarness.h"
#include <filesystem>

namespace Hyperion
{
namespace
{
void RoutingCheck(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Shortcut routing acceptance: ") + InMessage);
	}
}

void RoutingKey(std::vector<FInputEvent>& InEvents, EKey InKey, unsigned InModifiers = InputModifiers::None,
                bool bInRepeat = false)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.Modifiers = InModifiers;
	Event.bDown = true;
	Event.bRepeat = bInRepeat;
	InEvents.push_back(Event);
}

void RoutingClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = (InBounds.X + InBounds.Z) / 2;
	Event.Y = (InBounds.Y + InBounds.W) / 2;
	InEvents.push_back(Event);
	Event.Type = EEventType::MouseButton;
	Event.Button = InputButtons::Left;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

void RoutingText(std::vector<FInputEvent>& InEvents, const char* InText)
{
	FInputEvent Event;
	Event.Type = EEventType::Text;
	Event.Text = InText;
	InEvents.push_back(Event);
}
} // namespace

void FEditorAcceptanceHarness::ExerciseShortcutFocus(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ShortcutStep)
	{
		case 55:
		case 57:
			RoutingCheck(Editor.Gui->IsWindowFocused(Scenario.ShortcutStep == 55 ? "Content Browser" : "Log"),
			             "unrelated Delete focus fixture failed");
			RoutingKey(InEvents, EKey::Delete);
			break;
		case 56:
			CheckShortcutSelection({0});
			Editor.bShowLog = Editor.bFocusLog = true;
			Editor.Gui->FocusWindow("Log");
			break;
		case 58:
			CheckShortcutSelection({0});
			Editor.Gui->FocusWindow("Outliner");
			break;
		case 59:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = false;
			InEvents.push_back(Focus);
			RoutingKey(InEvents, EKey::Delete);
			break;
		}
		case 60:
		{
			CheckShortcutSelection({0});
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = true;
			InEvents.push_back(Focus);
			break;
		}
		case 61:
			// Newly activated search owns Delete during this same event batch.
			RoutingClick(InEvents, Scenario.InspectionBounds.at("clipboard/search"), true);
			RoutingKey(InEvents, EKey::Delete);
			break;
		case 62:
			RoutingClick(InEvents, Scenario.InspectionBounds.at("clipboard/search"), false);
			break;
		case 63:
			CheckShortcutSelection({0});
			Editor.Gui->FinishEditing();
			Editor.Gui->FocusWindow("Outliner");
			break;
		case 64:
			RoutingCheck(Editor.Gui->IsWindowFocused("Outliner"), "Outliner command focus fixture failed");
			Scenario.ShortcutObjectId = Editor.Scene->FindNode(Scenario.ShortcutObjects[0])->Id;
			RoutingKey(InEvents, EKey::Delete);
			break;
		case 65:
			RoutingCheck(!Editor.Scene->FindHandle(Scenario.ShortcutObjectId).Scene && Editor.HistoryCursor == 1,
			             "focused Outliner Delete was not one scene command");
			Editor.Gui->FocusWindow("Content Browser");
			break;
		case 66:
			RoutingCheck(Editor.Gui->IsWindowFocused("Content Browser"),
			             "Content Browser command focus fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			break;
		case 67:
			RoutingCheck(Editor.Scene->FindHandle(Scenario.ShortcutObjectId).Scene && Editor.HistoryCursor == 0 &&
			                 !Editor.IsDirty(),
			             "global Content Browser Undo failed");
			Scenario.ShortcutObjects[0] = Editor.Scene->FindHandle(Scenario.ShortcutObjectId);
			Editor.Gui->FocusWindow("Log");
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseShortcutHistory(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ShortcutStep)
	{
		case 68:
			RoutingCheck(Editor.Gui->IsWindowFocused("Log"), "Log command focus fixture failed");
			RoutingKey(InEvents, EKey::Y, InputModifiers::Control, true);
			break;
		case 69:
			RoutingCheck(!Editor.Scene->FindHandle(Scenario.ShortcutObjectId).Scene && Editor.HistoryCursor == 1,
			             "global repeated Log Redo failed");
			RoutingKey(InEvents, EKey::Tab);
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			break;
		case 70:
			RoutingCheck(Editor.HistoryCursor == 1, "ownership-changing Tab batch undid scene history");
			Editor.Gui->FinishEditing();
			Editor.Gui->FocusWindow("Log");
			break;
		case 71:
			RoutingCheck(Editor.Gui->IsWindowFocused("Log"), "Log command focus fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control, true);
			break;
		case 72:
		{
			RoutingCheck(Editor.Scene->FindHandle(Scenario.ShortcutObjectId).Scene && Editor.HistoryCursor == 0 &&
			                 !Editor.IsDirty(),
			             "global repeated Log Undo failed");
			Scenario.ShortcutObjects[0] = Editor.Scene->FindHandle(Scenario.ShortcutObjectId);
			Editor.ResetDocument();
			FSceneNode Sentinel;
			Sentinel.Name = "Text undo sentinel";
			Editor.CommitCreate(std::move(Sentinel));
			Editor.SelectObject(Scenario.ShortcutObjects[0]);
			Editor.Filter = "Shortcut";
			Editor.Gui->FocusWindow("Outliner");
			break;
		}
		case 73:
			RoutingClick(InEvents, Scenario.InspectionBounds.at("clipboard/search"), true);
			break;
		case 74:
			RoutingClick(InEvents, Scenario.InspectionBounds.at("clipboard/search"), false);
			break;
		case 75:
			RoutingKey(InEvents, EKey::End);
			break;
		case 76:
			RoutingText(InEvents, "x");
			break;
		case 77:
			RoutingCheck(Editor.Filter == "Shortcutx" && Editor.Gui->IsTextInputOwnedThisFrame(),
			             "search text fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			break;
		case 78:
			if (Editor.Filter != "Shortcut" || Editor.HistoryCursor != 1 || Editor.History.size() != 1)
			{
				throw std::runtime_error("Shortcut routing native Undo: filter=" + Editor.Filter +
				                         " cursor=" + std::to_string(Editor.HistoryCursor) +
				                         " entries=" + std::to_string(Editor.History.size()));
			}
			Editor.Gui->FinishEditing();
			Editor.Undo();
			Editor.ResetDocument();
			Editor.SelectObject(Scenario.ShortcutObjects[0]);
			Editor.Gui->FocusWindow("Details");
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseShortcutText(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ShortcutStep)
	{
		case 79:
			RoutingClick(InEvents, Scenario.InspectionBounds.at("shortcut/name"), true);
			break;
		case 80:
			RoutingClick(InEvents, Scenario.InspectionBounds.at("shortcut/name"), false);
			break;
		case 81:
			RoutingKey(InEvents, EKey::A, InputModifiers::Control);
			break;
		case 82:
			RoutingText(InEvents, "Shortcut renamed");
			break;
		case 83:
			RoutingCheck(Editor.Scene->FindNode(Scenario.ShortcutObjects[0])->Name == "Shortcut renamed" &&
			                 Editor.InspectorInteraction && Editor.HistoryCursor == 1,
			             "live inspector fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			break;
		case 84:
			RoutingCheck(Editor.Scene->FindNode(Scenario.ShortcutObjects[0])->Name == "Shortcut A" &&
			                 !Editor.InspectorTransaction && Editor.HistoryCursor == 0 && !Editor.IsDirty(),
			             "inspector Finish then Undo failed");
			break;
		case 85:
			RoutingCheck(Editor.Scene->FindNode(Scenario.ShortcutObjects[0])->Name == "Shortcut A",
			             "native text history overwrote inspector scene Undo on the next frame");
			Editor.ResetDocument();
			Scenario.ShortcutDocumentPath = Editor.CurrentPath;
			Editor.SceneDocument.SetPath((Editor.Options.Report.parent_path() / "ShortcutSaved.hasset").string());
			Editor.Gui->FocusWindow("Content Browser");
			break;
		case 86:
			RoutingKey(InEvents, EKey::S, InputModifiers::Control, true);
			break;
		case 87:
			RoutingCheck(Editor.Gui->IsWindowFocused("Content Browser"),
			             "Content Browser command focus fixture failed");
			RoutingCheck(!Editor.PendingSave && !std::filesystem::exists(Editor.CurrentPath),
			             "repeated Save was admitted");
			RoutingKey(InEvents, EKey::S, InputModifiers::Control);
			break;
		case 88:
			RoutingCheck(std::filesystem::exists(Editor.CurrentPath) && Editor.LastSaveMilliseconds > 0 &&
			                 Editor.Error.empty(),
			             "global Content Browser Save failed");
			Editor.SceneDocument.SetPath(Scenario.ShortcutDocumentPath);
			Editor.ResetDocument();
			Editor.Gui->FocusWindow("Outliner");
			break;
	}
}

bool FEditorAcceptanceHarness::ExerciseShortcutRouting(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.ShortcutStep == 88 && Editor.PendingSave)
	{
		return false;
	}
	if (Scenario.ShortcutStep <= 67)
	{
		ExerciseShortcutFocus(InEvents);
	}
	else if (Scenario.ShortcutStep <= 78)
	{
		ExerciseShortcutHistory(InEvents);
	}
	else if (Scenario.ShortcutStep <= 88)
	{
		ExerciseShortcutText(InEvents);
	}
	else
	{
		ExerciseShortcutPopup(InEvents);
	}
	return true;
}

void FEditorAcceptanceHarness::ExerciseShortcutPopup(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ShortcutStep)
	{
		case 89:
			RoutingCheck(Editor.Gui->IsWindowFocused("Outliner"), "popup focus fixture failed");
			break;
		case 90:
			RoutingKey(InEvents, EKey::Delete);
			break;
		case 91:
			RoutingCheck(Editor.Gui->HasOpenPopup() && Editor.Scene->FindNode(Scenario.ShortcutObjects[0]) &&
			                 Editor.History.empty(),
			             "ordinary popup admitted scene Delete");
			Editor.Gui->ClosePopups();
			break;
		case 92:
			Scenario.bSelectionShortcutsVerified = true;
			break;
	}
}

void FEditorAcceptanceHarness::DrawPanels() const
{
	constexpr unsigned PopupInputStep = 90;
	constexpr unsigned PopupCheckStep = 91;
	if (!Editor.Options.bExerciseSelectionShortcuts || Scenario.ShortcutStep < PopupInputStep + 1 ||
	    Scenario.ShortcutStep > PopupCheckStep)
	{
		return;
	}
	bool bOpen = true;
	Editor.Gui->BeginWindow("Shortcut popup fixture", bOpen);
	Editor.Gui->OpenPopup("Shortcut popup");
	if (Editor.Gui->BeginPopup("Shortcut popup"))
	{
		Editor.Gui->Text("Ordinary popup owns scene shortcuts");
		Editor.Gui->EndPopup();
	}
	Editor.Gui->EndWindow();
}
} // namespace Hyperion
