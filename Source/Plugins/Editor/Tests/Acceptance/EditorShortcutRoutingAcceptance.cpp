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
	switch (Scenario.Shortcut.Progress.GetState())
	{
		case EShortcutState::DeleteInContentBrowser:
		case EShortcutState::DeleteInLog:
			RoutingCheck(
			    Editor.Gui->IsWindowFocused(
			        Scenario.Shortcut.Progress.Is(EShortcutState::DeleteInContentBrowser) ? "Content Browser" : "Log"),
			    "unrelated Delete focus fixture failed");
			RoutingKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(
			    Scenario.Shortcut.Progress.Is(EShortcutState::DeleteInContentBrowser)
			        ? EShortcutState::VerifyContentDeleteGuard
			        : EShortcutState::VerifyLogDeleteGuard);
			break;
		case EShortcutState::VerifyContentDeleteGuard:
			CheckShortcutSelection({0});
			Editor.bShowLog = Editor.bFocusLog = true;
			Editor.Gui->FocusWindow("Log");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::DeleteInLog);
			break;
		case EShortcutState::VerifyLogDeleteGuard:
			CheckShortcutSelection({0});
			Editor.Gui->FocusWindow("Outliner");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::DeleteDuringFocusLoss);
			break;
		case EShortcutState::DeleteDuringFocusLoss:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = false;
			InEvents.push_back(Focus);
			RoutingKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::RestoreFocus);
			break;
		}
		case EShortcutState::RestoreFocus:
		{
			CheckShortcutSelection({0});
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = true;
			InEvents.push_back(Focus);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::DeleteOnSearchActivation);
			break;
		}
		case EShortcutState::DeleteOnSearchActivation:
			// Newly activated search owns Delete during this same event batch.
			RoutingClick(InEvents, Scenario.Bounds.Require(EEditorWidget::OutlinerSearch), true);
			RoutingKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReleaseActivatedSearch);
			break;
		case EShortcutState::ReleaseActivatedSearch:
			RoutingClick(InEvents, Scenario.Bounds.Require(EEditorWidget::OutlinerSearch), false);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyActivationGuard);
			break;
		case EShortcutState::VerifyActivationGuard:
			CheckShortcutSelection({0});
			Editor.Gui->FinishEditing();
			Editor.Gui->FocusWindow("Outliner");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::DeleteInOutliner);
			break;
		case EShortcutState::DeleteInOutliner:
			RoutingCheck(Editor.Gui->IsWindowFocused("Outliner"), "Outliner command focus fixture failed");
			Scenario.ShortcutObjectId = Editor.Scene->FindNode(Scenario.ShortcutObjects[0])->Id;
			RoutingKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyOutlinerDelete);
			break;
		case EShortcutState::VerifyOutlinerDelete:
			RoutingCheck(!Editor.Scene->FindHandle(Scenario.ShortcutObjectId).Scene && Editor.HistoryCursor == 1,
			             "focused Outliner Delete was not one scene command");
			Editor.Gui->FocusWindow("Content Browser");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::UndoInContentBrowser);
			break;
		case EShortcutState::UndoInContentBrowser:
			RoutingCheck(Editor.Gui->IsWindowFocused("Content Browser"),
			             "Content Browser command focus fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyContentUndo);
			break;
		case EShortcutState::VerifyContentUndo:
			RoutingCheck(Editor.Scene->FindHandle(Scenario.ShortcutObjectId).Scene && Editor.HistoryCursor == 0 &&
			                 !Editor.IsDirty(),
			             "global Content Browser Undo failed");
			Scenario.ShortcutObjects[0] = Editor.Scene->FindHandle(Scenario.ShortcutObjectId);
			Editor.Gui->FocusWindow("Log");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::RepeatRedoInLog);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseShortcutHistory(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Shortcut.Progress.GetState())
	{
		case EShortcutState::RepeatRedoInLog:
			RoutingCheck(Editor.Gui->IsWindowFocused("Log"), "Log command focus fixture failed");
			RoutingKey(InEvents, EKey::Y, InputModifiers::Control, true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyLogRedoAndTabUndo);
			break;
		case EShortcutState::VerifyLogRedoAndTabUndo:
			RoutingCheck(!Editor.Scene->FindHandle(Scenario.ShortcutObjectId).Scene && Editor.HistoryCursor == 1,
			             "global repeated Log Redo failed");
			RoutingKey(InEvents, EKey::Tab);
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyTabGuard);
			break;
		case EShortcutState::VerifyTabGuard:
			RoutingCheck(Editor.HistoryCursor == 1, "ownership-changing Tab batch undid scene history");
			Editor.Gui->FinishEditing();
			Editor.Gui->FocusWindow("Log");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::RepeatUndoInLog);
			break;
		case EShortcutState::RepeatUndoInLog:
			RoutingCheck(Editor.Gui->IsWindowFocused("Log"), "Log command focus fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control, true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyLogUndoAndPrepareText);
			break;
		case EShortcutState::VerifyLogUndoAndPrepareText:
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
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::PressTextSearch);
			break;
		}
		case EShortcutState::PressTextSearch:
			RoutingClick(InEvents, Scenario.Bounds.Require(EEditorWidget::OutlinerSearch), true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReleaseTextSearch);
			break;
		case EShortcutState::ReleaseTextSearch:
			RoutingClick(InEvents, Scenario.Bounds.Require(EEditorWidget::OutlinerSearch), false);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::MoveTextCaret);
			break;
		case EShortcutState::MoveTextCaret:
			RoutingKey(InEvents, EKey::End);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::TypeSearchSuffix);
			break;
		case EShortcutState::TypeSearchSuffix:
			RoutingText(InEvents, "x");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::UndoSearchText);
			break;
		case EShortcutState::UndoSearchText:
			RoutingCheck(Editor.Filter == "Shortcutx" && Editor.Gui->IsTextInputOwnedThisFrame(),
			             "search text fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyTextUndo);
			break;
		case EShortcutState::VerifyTextUndo:
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
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::PressInspectorName);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseShortcutText(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Shortcut.Progress.GetState())
	{
		case EShortcutState::PressInspectorName:
			RoutingClick(InEvents, Scenario.Bounds.Require(EEditorWidget::ObjectName), true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReleaseInspectorName);
			break;
		case EShortcutState::ReleaseInspectorName:
			RoutingClick(InEvents, Scenario.Bounds.Require(EEditorWidget::ObjectName), false);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::SelectInspectorName);
			break;
		case EShortcutState::SelectInspectorName:
			RoutingKey(InEvents, EKey::A, InputModifiers::Control);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::TypeInspectorName);
			break;
		case EShortcutState::TypeInspectorName:
			RoutingText(InEvents, "Shortcut renamed");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::UndoInspectorEdit);
			break;
		case EShortcutState::UndoInspectorEdit:
			RoutingCheck(Editor.Scene->FindNode(Scenario.ShortcutObjects[0])->Name == "Shortcut renamed" &&
			                 Editor.InspectorInteraction && Editor.HistoryCursor == 1,
			             "live inspector fixture failed");
			RoutingKey(InEvents, EKey::Z, InputModifiers::Control);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyInspectorUndo);
			break;
		case EShortcutState::VerifyInspectorUndo:
			RoutingCheck(Editor.Scene->FindNode(Scenario.ShortcutObjects[0])->Name == "Shortcut A" &&
			                 !Editor.InspectorTransaction && Editor.HistoryCursor == 0 && !Editor.IsDirty(),
			             "inspector Finish then Undo failed");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifySettledInspectorUndo);
			break;
		case EShortcutState::VerifySettledInspectorUndo:
			RoutingCheck(Editor.Scene->FindNode(Scenario.ShortcutObjects[0])->Name == "Shortcut A",
			             "native text history overwrote inspector scene Undo on the next frame");
			Editor.ResetDocument();
			Scenario.ShortcutDocumentPath = Editor.CurrentPath;
			Editor.SceneDocument.SetPath((Editor.Options.Report.parent_path() / "ShortcutSaved.hasset").string());
			Editor.Gui->FocusWindow("Content Browser");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::RepeatSave);
			break;
		case EShortcutState::RepeatSave:
			RoutingKey(InEvents, EKey::S, InputModifiers::Control, true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyRepeatedSaveAndSave);
			break;
		case EShortcutState::VerifyRepeatedSaveAndSave:
			RoutingCheck(Editor.Gui->IsWindowFocused("Content Browser"),
			             "Content Browser command focus fixture failed");
			RoutingCheck(!Editor.PendingSave && !std::filesystem::exists(Editor.CurrentPath),
			             "repeated Save was admitted");
			RoutingKey(InEvents, EKey::S, InputModifiers::Control);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifySave);
			break;
		case EShortcutState::VerifySave:
			RoutingCheck(std::filesystem::exists(Editor.CurrentPath) && Editor.LastSaveMilliseconds > 0 &&
			                 Editor.Error.empty(),
			             "global Content Browser Save failed");
			Editor.SceneDocument.SetPath(Scenario.ShortcutDocumentPath);
			Editor.ResetDocument();
			Editor.Gui->FocusWindow("Outliner");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::PreparePopup);
			break;
	}
}

bool FEditorAcceptanceHarness::ExerciseShortcutRouting(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.Shortcut.Progress.Is(EShortcutState::VerifySave) && Editor.PendingSave)
	{
		return false;
	}
	if (Scenario.Shortcut.Progress.IsAny({EShortcutState::DeleteInContentBrowser,
	                                      EShortcutState::VerifyContentDeleteGuard, EShortcutState::DeleteInLog,
	                                      EShortcutState::VerifyLogDeleteGuard, EShortcutState::DeleteDuringFocusLoss,
	                                      EShortcutState::RestoreFocus, EShortcutState::DeleteOnSearchActivation,
	                                      EShortcutState::ReleaseActivatedSearch, EShortcutState::VerifyActivationGuard,
	                                      EShortcutState::DeleteInOutliner, EShortcutState::VerifyOutlinerDelete,
	                                      EShortcutState::UndoInContentBrowser, EShortcutState::VerifyContentUndo}))
	{
		ExerciseShortcutFocus(InEvents);
	}
	else if (Scenario.Shortcut.Progress.IsAny({EShortcutState::RepeatRedoInLog, EShortcutState::VerifyLogRedoAndTabUndo,
	                                           EShortcutState::VerifyTabGuard, EShortcutState::RepeatUndoInLog,
	                                           EShortcutState::VerifyLogUndoAndPrepareText,
	                                           EShortcutState::PressTextSearch, EShortcutState::ReleaseTextSearch,
	                                           EShortcutState::MoveTextCaret, EShortcutState::TypeSearchSuffix,
	                                           EShortcutState::UndoSearchText, EShortcutState::VerifyTextUndo}))
	{
		ExerciseShortcutHistory(InEvents);
	}
	else if (Scenario.Shortcut.Progress.IsAny({EShortcutState::PressInspectorName, EShortcutState::ReleaseInspectorName,
	                                           EShortcutState::SelectInspectorName, EShortcutState::TypeInspectorName,
	                                           EShortcutState::UndoInspectorEdit, EShortcutState::VerifyInspectorUndo,
	                                           EShortcutState::VerifySettledInspectorUndo, EShortcutState::RepeatSave,
	                                           EShortcutState::VerifyRepeatedSaveAndSave, EShortcutState::VerifySave}))
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
	switch (Scenario.Shortcut.Progress.GetState())
	{
		case EShortcutState::PreparePopup:
			RoutingCheck(Editor.Gui->IsWindowFocused("Outliner"), "popup focus fixture failed");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::DeleteDuringPopup);
			break;
		case EShortcutState::DeleteDuringPopup:
			RoutingKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyPopupGuard);
			break;
		case EShortcutState::VerifyPopupGuard:
			RoutingCheck(Editor.Gui->HasOpenPopup() && Editor.Scene->FindNode(Scenario.ShortcutObjects[0]) &&
			                 Editor.History.empty(),
			             "ordinary popup admitted scene Delete");
			Editor.Gui->ClosePopups();
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::Complete);
			break;
		case EShortcutState::Complete:
			Scenario.bSelectionShortcutsVerified = true;
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::Finished);
			break;
	}
}

void FEditorAcceptanceHarness::DrawPanels() const
{
	if (!Editor.Options.bExerciseSelectionShortcuts || !Scenario.Shortcut.Progress.Is(EShortcutState::VerifyPopupGuard))
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
