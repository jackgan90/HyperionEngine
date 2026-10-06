#include "EditorAcceptanceHarness.h"

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

void FEditorAcceptanceHarness::PrepareClipboardExercise()
{
	FSceneNode Parent;
	Parent.Id = "clipboard-parent";
	Parent.Name = "Clipboard group";
	auto CameraNode = MakeSceneCameraNode("clipboard-camera");
	CameraNode.Name = "Clipboard camera";
	CameraNode.Parent() = Parent.Id;
	const auto Models = Editor.Scene->GetNodes(ESceneNodeKind::Model);
	RequireClipboard(!Models.empty(), "fixture needs a resolved model");
	auto Model = *Editor.Scene->FindNode(Models.front());
	Model.Id = "clipboard-model";
	Model.Name = "Clipboard model";
	Model.Parent() = Parent.Id;
	if (auto& Source = Model.Components.Slot<FSceneModelSource>(); Source)
	{
		Source->InstanceRoot = Parent.Id;
	}
	const auto Nodes = Editor.Scene->AddNodes({Parent, CameraNode, Model});
	Editor.ResetDocument();
	FSceneSelection Chosen(Nodes[0]);
	Chosen.Toggle(Nodes[1]);
	Editor.SetSelection(std::move(Chosen));
	Scenario.ClipboardExerciseCount = Editor.Scene->GetNodes().size();
	Editor.Window->SetClipboard("initial ordinary text");
	Editor.Gui->FocusWindow("Outliner");
}

void FEditorAcceptanceHarness::ExerciseClipboardHistory(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Clipboard.Progress.GetState())
	{
		case EClipboardState::RepeatCopy:
			ClipboardKey(InEvents, EKey::C, true, true);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::CopySelection);
			break;
		case EClipboardState::CopySelection:
			RequireClipboard(Editor.Window->TypedClipboard(SceneClipboardFormat).empty(), "repeat copied objects");
			ClipboardKey(InEvents, EKey::C);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::VerifyCopyAndPaste);
			break;
		case EClipboardState::VerifyCopyAndPaste:
			RequireClipboard(Editor.SceneDocument.ClipboardInfo().Nodes == 3 && !Editor.IsDirty() &&
			                     Editor.History.empty(),
			                 "copy snapshot or history: " + Editor.Error);
			ClipboardKey(InEvents, EKey::V);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::VerifyPasteAndRepeat);
			break;
		case EClipboardState::VerifyPasteAndRepeat:
			RequireClipboard(Editor.Scene->GetNodes().size() == Scenario.ClipboardExerciseCount + 3 &&
			                     Editor.HistoryCursor == 1 && Editor.Selection.All().size() == 2,
			                 "first paste batch: " + Editor.Error);
			RequireClipboard(Editor.Scene->FindNode(*Editor.Selection)->Name == "Clipboard camera (1)",
			                 "numbered name");
			ClipboardKey(InEvents, EKey::V, true, true);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::PasteAgain);
			break;
		case EClipboardState::PasteAgain:
			RequireClipboard(Editor.HistoryCursor == 1, "repeat pasted objects");
			ClipboardKey(InEvents, EKey::V);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::VerifySecondPasteAndReplaceClipboard);
			break;
		case EClipboardState::VerifySecondPasteAndReplaceClipboard:
			RequireClipboard(Editor.Scene->GetNodes().size() == Scenario.ClipboardExerciseCount + 6 &&
			                     Editor.HistoryCursor == 2,
			                 "second paste batch: " + Editor.Error);
			Editor.Undo();
			Editor.Window->SetClipboard("ordinary text");
			ClipboardKey(InEvents, EKey::V);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::VerifyTextReplacementAndRedo);
			break;
		case EClipboardState::VerifyTextReplacementAndRedo:
			RequireClipboard(Editor.Scene->GetNodes().size() == Scenario.ClipboardExerciseCount + 3 &&
			                     Editor.HistoryCursor == 1,
			                 "text replacement pasted stale objects");
			Editor.Redo();
			RequireClipboard(Editor.Scene->GetNodes().size() == Scenario.ClipboardExerciseCount + 6 &&
			                     Editor.HistoryCursor == 2,
			                 "redo read the clipboard");
			Editor.SceneDocument.CopySelection(Editor.SceneDocument.Id(), Editor.Scene->GetRevision());
			Editor.Gui->FocusWindow("Content Browser");
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::PasteInContentBrowser);
			break;
		case EClipboardState::PasteInContentBrowser:
			ClipboardKey(InEvents, EKey::V);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::VerifyContentBrowserAndPressSearch);
			break;
		case EClipboardState::VerifyContentBrowserAndPressSearch:
			RequireClipboard(Editor.HistoryCursor == 2, "Content Browser pasted scene nodes");
			Editor.Gui->FocusWindow("Outliner");
			ClipboardClick(InEvents, Scenario.InspectionBounds.at("clipboard/search"), true);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::ReleaseSearch);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseClipboardText(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Clipboard.Progress.GetState())
	{
		case EClipboardState::ReleaseSearch:
			ClipboardClick(InEvents, Scenario.InspectionBounds.at("clipboard/search"), false);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::SelectSearchText);
			break;
		case EClipboardState::SelectSearchText:
			RequireClipboard(Editor.Gui->IsEditingText(), "search did not own text input");
			ClipboardKey(InEvents, EKey::A);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::PasteSearchText);
			break;
		case EClipboardState::PasteSearchText:
			ClipboardKey(InEvents, EKey::V);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::VerifySearchAndSelectText);
			break;
		case EClipboardState::VerifySearchAndSelectText:
			RequireClipboard(Editor.HistoryCursor == 2 && Editor.Filter.find("Clipboard") != std::string::npos,
			                 "text paste did not remain in search");
			ClipboardKey(InEvents, EKey::A);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::CopySearchText);
			break;
		case EClipboardState::CopySearchText:
			ClipboardKey(InEvents, EKey::C);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::VerifyTextCopyAndFocusViewport);
			break;
		case EClipboardState::VerifyTextCopyAndFocusViewport:
			RequireClipboard(Editor.Window->TypedClipboard(SceneClipboardFormat).empty(),
			                 "GUI text copy retained token");
			Editor.Gui->FinishEditing();
			Editor.Gui->FocusWindow("Viewport");
			Editor.Filter.clear();
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::PasteInViewport);
			break;
		case EClipboardState::PasteInViewport:
			ClipboardKey(InEvents, EKey::V);
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::VerifyViewportPaste);
			break;
		case EClipboardState::VerifyViewportPaste:
			RequireClipboard(Editor.HistoryCursor == 2 &&
			                     Editor.Scene->GetNodes().size() == Scenario.ClipboardExerciseCount + 6,
			                 "viewport pasted old object snapshot");
			Editor.Error.clear();
			Scenario.bClipboardVerified = true;
			Scenario.Clipboard.Progress.TransitionTo(EClipboardState::Complete);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseClipboard(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Scene->GetStatus().bReady || !Editor.Viewport.bViewportVisible || Scenario.bClipboardVerified)
	{
		return;
	}
	const auto InputPhase = Scenario.Clipboard.Cadence.Advance();
	if (InputPhase != EAcceptanceCadencePhase::Execute)
	{
		if (InputPhase == EAcceptanceCadencePhase::ReleaseKeys)
		{
			for (const auto Key : {EKey::A, EKey::C, EKey::V})
			{
				ClipboardKey(InEvents, Key, false);
			}
		}
		return;
	}
	if (Scenario.Clipboard.Progress.Is(EClipboardState::PrepareClipboard))
	{
		PrepareClipboardExercise();
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
		Scenario.Clipboard.Progress.TransitionTo(EClipboardState::RepeatCopy);
	}
	else if (Scenario.Clipboard.Progress.IsAny(
	             {EClipboardState::RepeatCopy, EClipboardState::CopySelection, EClipboardState::VerifyCopyAndPaste,
	              EClipboardState::VerifyPasteAndRepeat, EClipboardState::PasteAgain,
	              EClipboardState::VerifySecondPasteAndReplaceClipboard, EClipboardState::VerifyTextReplacementAndRedo,
	              EClipboardState::PasteInContentBrowser, EClipboardState::VerifyContentBrowserAndPressSearch}))
	{
		ExerciseClipboardHistory(InEvents);
	}
	else
	{
		ExerciseClipboardText(InEvents);
	}
}
} // namespace Hyperion
