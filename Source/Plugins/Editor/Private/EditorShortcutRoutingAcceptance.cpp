#include "EditorApplication.h"
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

void FEditorPlugin::ExerciseShortcutFocus(std::vector<FInputEvent>& InEvents)
{
	switch (Acceptance.ShortcutStep)
	{
		case 55:
		case 57:
			RoutingCheck(Gui->IsWindowFocused(Acceptance.ShortcutStep == 55 ? "Content Browser" : "Log"),
			             "unrelated Delete focus fixture failed");
			RoutingKey(InEvents, EKey::Delete);
			break;
		case 56:
			CheckShortcutSelection({0});
			bShowLog = bFocusLog = true;
			Gui->FocusWindow("Log");
			break;
		case 58:
			CheckShortcutSelection({0});
			Gui->FocusWindow("Outliner");
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
			RoutingClick(InEvents, InspectionBounds.at("clipboard/search"), true);
			RoutingKey(InEvents, EKey::Delete);
			break;
		case 62:
			RoutingClick(InEvents, InspectionBounds.at("clipboard/search"), false);
			break;
		case 63:
			CheckShortcutSelection({0});
			Gui->FinishEditing();
			Gui->FocusWindow("Outliner");
			break;
		case 64:
			RoutingCheck(Gui->IsWindowFocused("Outliner"), "Outliner command focus fixture failed");
			Acceptance.ShortcutObjectId = Scene->FindNode(Acceptance.ShortcutObjects[0])->Id;
			RoutingKey(InEvents, EKey::Delete);
			break;
		case 65:
			RoutingCheck(!Scene->FindHandle(Acceptance.ShortcutObjectId).Scene && HistoryCursor == 1,
			             "focused Outliner Delete was not one scene command");
			Gui->FocusWindow("Content Browser");
			break;
		case 66:
			RoutingCheck(Gui->IsWindowFocused("Content Browser"), "Content Browser command focus fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			break;
		case 67:
			RoutingCheck(Scene->FindHandle(Acceptance.ShortcutObjectId).Scene && HistoryCursor == 0 && !IsDirty(),
			             "global Content Browser Undo failed");
			Acceptance.ShortcutObjects[0] = Scene->FindHandle(Acceptance.ShortcutObjectId);
			Gui->FocusWindow("Log");
			break;
	}
}

void FEditorPlugin::ExerciseShortcutHistory(std::vector<FInputEvent>& InEvents)
{
	switch (Acceptance.ShortcutStep)
	{
		case 68:
			RoutingCheck(Gui->IsWindowFocused("Log"), "Log command focus fixture failed");
			RoutingKey(InEvents, EKey::Y, InputModifiers::Control, true);
			break;
		case 69:
			RoutingCheck(!Scene->FindHandle(Acceptance.ShortcutObjectId).Scene && HistoryCursor == 1,
			             "global repeated Log Redo failed");
			RoutingKey(InEvents, EKey::Tab);
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			break;
		case 70:
			RoutingCheck(HistoryCursor == 1, "ownership-changing Tab batch undid scene history");
			Gui->FinishEditing();
			Gui->FocusWindow("Log");
			break;
		case 71:
			RoutingCheck(Gui->IsWindowFocused("Log"), "Log command focus fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control, true);
			break;
		case 72:
		{
			RoutingCheck(Scene->FindHandle(Acceptance.ShortcutObjectId).Scene && HistoryCursor == 0 && !IsDirty(),
			             "global repeated Log Undo failed");
			Acceptance.ShortcutObjects[0] = Scene->FindHandle(Acceptance.ShortcutObjectId);
			ResetDocument();
			FSceneNode Sentinel;
			Sentinel.Name = "Text undo sentinel";
			CommitCreate(std::move(Sentinel));
			SelectObject(Acceptance.ShortcutObjects[0]);
			Filter = "Shortcut";
			Gui->FocusWindow("Outliner");
			break;
		}
		case 73:
			RoutingClick(InEvents, InspectionBounds.at("clipboard/search"), true);
			break;
		case 74:
			RoutingClick(InEvents, InspectionBounds.at("clipboard/search"), false);
			break;
		case 75:
			RoutingKey(InEvents, EKey::End);
			break;
		case 76:
			RoutingText(InEvents, "x");
			break;
		case 77:
			RoutingCheck(Filter == "Shortcutx" && Gui->IsTextInputOwnedThisFrame(), "search text fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			break;
		case 78:
			if (Filter != "Shortcut" || HistoryCursor != 1 || History.size() != 1)
			{
				throw std::runtime_error("Shortcut routing native Undo: filter=" + Filter + " cursor=" +
				                         std::to_string(HistoryCursor) + " entries=" + std::to_string(History.size()));
			}
			Gui->FinishEditing();
			Undo();
			ResetDocument();
			SelectObject(Acceptance.ShortcutObjects[0]);
			Gui->FocusWindow("Details");
			break;
	}
}

void FEditorPlugin::ExerciseShortcutText(std::vector<FInputEvent>& InEvents)
{
	switch (Acceptance.ShortcutStep)
	{
		case 79:
			RoutingClick(InEvents, InspectionBounds.at("shortcut/name"), true);
			break;
		case 80:
			RoutingClick(InEvents, InspectionBounds.at("shortcut/name"), false);
			break;
		case 81:
			RoutingKey(InEvents, EKey::A, InputModifiers::Control);
			break;
		case 82:
			RoutingText(InEvents, "Shortcut renamed");
			break;
		case 83:
			RoutingCheck(Scene->FindNode(Acceptance.ShortcutObjects[0])->Name == "Shortcut renamed" &&
			                 InspectorInteraction && HistoryCursor == 1,
			             "live inspector fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			break;
		case 84:
			RoutingCheck(Scene->FindNode(Acceptance.ShortcutObjects[0])->Name == "Shortcut A" &&
			                 !InspectorTransaction && HistoryCursor == 0 && !IsDirty(),
			             "inspector Finish then Undo failed");
			break;
		case 85:
			RoutingCheck(Scene->FindNode(Acceptance.ShortcutObjects[0])->Name == "Shortcut A",
			             "native text history overwrote inspector scene Undo on the next frame");
			ResetDocument();
			Acceptance.ShortcutDocumentPath = CurrentPath;
			SceneDocument.SetPath((Options.Report.parent_path() / "ShortcutSaved.hasset").string());
			Gui->FocusWindow("Content Browser");
			break;
		case 86:
			RoutingKey(InEvents, EKey::S, InputModifiers::Control, true);
			break;
		case 87:
			RoutingCheck(Gui->IsWindowFocused("Content Browser"), "Content Browser command focus fixture failed");
			RoutingCheck(!PendingSave && !std::filesystem::exists(CurrentPath), "repeated Save was admitted");
			RoutingKey(InEvents, EKey::S, InputModifiers::Control);
			break;
		case 88:
			RoutingCheck(std::filesystem::exists(CurrentPath) && LastSaveMilliseconds > 0 && Error.empty(),
			             "global Content Browser Save failed");
			SceneDocument.SetPath(Acceptance.ShortcutDocumentPath);
			ResetDocument();
			Gui->FocusWindow("Outliner");
			break;
	}
}

bool FEditorPlugin::ExerciseShortcutRouting(std::vector<FInputEvent>& InEvents)
{
	if (Acceptance.ShortcutStep == 88 && PendingSave)
	{
		return false;
	}
	if (Acceptance.ShortcutStep <= 67)
	{
		ExerciseShortcutFocus(InEvents);
	}
	else if (Acceptance.ShortcutStep <= 78)
	{
		ExerciseShortcutHistory(InEvents);
	}
	else if (Acceptance.ShortcutStep <= 88)
	{
		ExerciseShortcutText(InEvents);
	}
	else
	{
		ExerciseShortcutPopup(InEvents);
	}
	return true;
}

void FEditorPlugin::ExerciseShortcutPopup(std::vector<FInputEvent>& InEvents)
{
	switch (Acceptance.ShortcutStep)
	{
		case 89:
			RoutingCheck(Gui->IsWindowFocused("Outliner"), "popup focus fixture failed");
			break;
		case 90:
			RoutingKey(InEvents, EKey::Delete);
			break;
		case 91:
			RoutingCheck(Gui->HasOpenPopup() && Scene->FindNode(Acceptance.ShortcutObjects[0]) && History.empty(),
			             "ordinary popup admitted scene Delete");
			Gui->ClosePopups();
			break;
		case 92:
			Acceptance.bSelectionShortcutsVerified = true;
			break;
	}
}

void FEditorAcceptanceDriver::DrawPanels(FEditorPlugin& InEditor) const
{
	constexpr unsigned PopupInputStep = 90;
	constexpr unsigned PopupCheckStep = 91;
	if (!InEditor.Options.bExerciseSelectionShortcuts || ShortcutStep < PopupInputStep + 1 ||
	    ShortcutStep > PopupCheckStep)
	{
		return;
	}
	bool bOpen = true;
	InEditor.Gui->BeginWindow("Shortcut popup fixture", bOpen);
	InEditor.Gui->OpenPopup("Shortcut popup");
	if (InEditor.Gui->BeginPopup("Shortcut popup"))
	{
		InEditor.Gui->Text("Ordinary popup owns scene shortcuts");
		InEditor.Gui->EndPopup();
	}
	InEditor.Gui->EndWindow();
}
} // namespace Hyperion
