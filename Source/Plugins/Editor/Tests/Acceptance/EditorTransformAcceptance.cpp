#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Math/AffineTransform.h"
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
void TypeValue(std::vector<FInputEvent>& InEvents, ETransformTextAction InPhase, const char* InText)
{
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Key = (InPhase == ETransformTextAction::FocusField || InPhase == ETransformTextAction::SelectAllPress ||
	           InPhase == ETransformTextAction::TypeValue)
	              ? EKey::A
	              : EKey::Enter;
	Key.bDown = InPhase == ETransformTextAction::SelectAllPress || InPhase == ETransformTextAction::ConfirmPress;
	Key.Modifiers = InPhase == ETransformTextAction::SelectAllPress ? InputModifiers::Control : InputModifiers::None;
	InEvents.push_back(Key);
	if (InPhase == ETransformTextAction::TypeValue)
	{
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = InText;
		InEvents.push_back(std::move(Text));
	}
}

struct FTransformHistoryCommand
{
	EKey Key;
	bool bDown;
	ETransformState Destination;
};

void SendTransformHistory(std::vector<FInputEvent>& InEvents, const FTransformHistoryCommand& InCommand)
{
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Key = InCommand.Key;
	Key.Modifiers = InputModifiers::Control;
	Key.bDown = InCommand.bDown;
	InEvents.push_back(Key);
}

FTransformHistoryCommand DescribeTransformHistoryCommand(ETransformState InState)
{
	switch (InState)
	{
		case ETransformState::UndoScalePress:
			return {EKey::Z, true, ETransformState::UndoScaleRelease};
		case ETransformState::UndoScaleRelease:
			return {EKey::Z, false, ETransformState::UndoRotationPress};
		case ETransformState::UndoRotationPress:
			return {EKey::Z, true, ETransformState::UndoRotationRelease};
		case ETransformState::UndoRotationRelease:
			return {EKey::Z, false, ETransformState::UndoPositionPress};
		case ETransformState::UndoPositionPress:
			return {EKey::Z, true, ETransformState::UndoPositionRelease};
		case ETransformState::UndoPositionRelease:
			return {EKey::Z, false, ETransformState::VerifyBaselineAndRedoPositionPress};
		case ETransformState::VerifyBaselineAndRedoPositionPress:
			return {EKey::Y, true, ETransformState::RedoPositionRelease};
		case ETransformState::RedoPositionRelease:
			return {EKey::Y, false, ETransformState::RedoRotationPress};
		case ETransformState::RedoRotationPress:
			return {EKey::Y, true, ETransformState::RedoRotationRelease};
		case ETransformState::RedoRotationRelease:
			return {EKey::Y, false, ETransformState::RedoScalePress};
		case ETransformState::RedoScalePress:
			return {EKey::Y, true, ETransformState::RedoScaleRelease};
		case ETransformState::RedoScaleRelease:
			return {EKey::Y, false, ETransformState::VerifyEditedTransformAndUndoScalePress};
		case ETransformState::VerifyEditedTransformAndUndoScalePress:
			return {EKey::Z, true, ETransformState::SecondUndoScaleRelease};
		case ETransformState::SecondUndoScaleRelease:
			return {EKey::Z, false, ETransformState::SecondUndoRotationPress};
		case ETransformState::SecondUndoRotationPress:
			return {EKey::Z, true, ETransformState::SecondUndoRotationRelease};
		case ETransformState::SecondUndoRotationRelease:
			return {EKey::Z, false, ETransformState::SecondUndoPositionPress};
		case ETransformState::SecondUndoPositionPress:
			return {EKey::Z, true, ETransformState::SecondUndoPositionRelease};
		case ETransformState::SecondUndoPositionRelease:
			return {EKey::Z, false, ETransformState::VerifyRestoredTransform};
		case ETransformState::VerifyOriginalValueAndUndoPress:
			return {EKey::Z, true, ETransformState::UndoOriginalValueRelease};
		case ETransformState::UndoOriginalValueRelease:
			return {EKey::Z, false, ETransformState::VerifyUnchangedUndoAndRedoPress};
		case ETransformState::VerifyUnchangedUndoAndRedoPress:
			return {EKey::Y, true, ETransformState::RedoOriginalValueRelease};
		case ETransformState::RedoOriginalValueRelease:
			return {EKey::Y, false, ETransformState::VerifyOriginalValueAndUndoAgainPress};
		case ETransformState::VerifyOriginalValueAndUndoAgainPress:
			return {EKey::Z, true, ETransformState::UndoOriginalValueAgainRelease};
		case ETransformState::UndoOriginalValueAgainRelease:
			return {EKey::Z, false, ETransformState::BeginNumericDrag};
		default:
			throw std::logic_error("Invalid transform history command");
	}
}

} // namespace

bool FEditorAcceptanceHarness::ExerciseTransformInput(std::vector<FInputEvent>& InEvents)
{
	const auto Type = RecordType<FSceneTransform>().Id;
	if (IsTransformTextState(Scenario.Transform.Progress.GetState()))
	{
		const unsigned Field = DescribeTransformTextContext(Scenario.Transform.Progress.GetState()).CaseIndex;
		const auto Phase = DescribeTransformTextContext(Scenario.Transform.Progress.GetState()).Action;
		const std::array Ids{"position/x", "rotation/z", "scale/y"};
		const std::array Values{"2", "45", "1.25"};
		if (Phase == ETransformTextAction::FocusField)
		{
			if (!ExerciseClick(InEvents, Scenario.Bounds.Require(FPropertyKey{Type, Ids[Field]}),
			                   Scenario.Transform.Click))
			{
				return false;
			}
		}
		else
		{
			// ImGui may deliver the queued key release and text on separate frames.
			if (Phase == ETransformTextAction::ConfirmPress && !Scenario.Transform.TextCommitObservation.Advance())
			{
				return false;
			}
			if (Phase == ETransformTextAction::ConfirmPress && (!Editor.IsDirty() || Editor.HistoryCursor != Field + 1))
			{
				throw std::runtime_error("Inspector input was not live before Enter: field=" + std::to_string(Field) +
				                         " history=" + std::to_string(Editor.HistoryCursor) + " active=" +
				                         std::to_string(Editor.InspectorInteraction) + " error=" + Editor.Error);
			}
			Scenario.Transform.TextCommitObservation.Restart();
			TypeValue(InEvents, Phase, Values[Field]);
		}
		switch (Scenario.Transform.Progress.GetState())
		{
			case ETransformState::PositionFocusField:
				Scenario.Transform.Progress.TransitionTo(ETransformState::PositionSelectAllPress);
				break;
			case ETransformState::PositionSelectAllPress:
				Scenario.Transform.Progress.TransitionTo(ETransformState::PositionTypeValue);
				break;
			case ETransformState::PositionTypeValue:
				Scenario.Transform.Progress.TransitionTo(ETransformState::PositionConfirmPress);
				break;
			case ETransformState::PositionConfirmPress:
				Scenario.Transform.Progress.TransitionTo(ETransformState::PositionConfirmRelease);
				break;
			case ETransformState::PositionConfirmRelease:
				Scenario.Transform.Progress.TransitionTo(ETransformState::RotationFocusField);
				break;
			case ETransformState::RotationFocusField:
				Scenario.Transform.Progress.TransitionTo(ETransformState::RotationSelectAllPress);
				break;
			case ETransformState::RotationSelectAllPress:
				Scenario.Transform.Progress.TransitionTo(ETransformState::RotationTypeValue);
				break;
			case ETransformState::RotationTypeValue:
				Scenario.Transform.Progress.TransitionTo(ETransformState::RotationConfirmPress);
				break;
			case ETransformState::RotationConfirmPress:
				Scenario.Transform.Progress.TransitionTo(ETransformState::RotationConfirmRelease);
				break;
			case ETransformState::RotationConfirmRelease:
				Scenario.Transform.Progress.TransitionTo(ETransformState::ScaleFocusField);
				break;
			case ETransformState::ScaleFocusField:
				Scenario.Transform.Progress.TransitionTo(ETransformState::ScaleSelectAllPress);
				break;
			case ETransformState::ScaleSelectAllPress:
				Scenario.Transform.Progress.TransitionTo(ETransformState::ScaleTypeValue);
				break;
			case ETransformState::ScaleTypeValue:
				Scenario.Transform.Progress.TransitionTo(ETransformState::ScaleConfirmPress);
				break;
			case ETransformState::ScaleConfirmPress:
				Scenario.Transform.Progress.TransitionTo(ETransformState::ScaleConfirmRelease);
				break;
			case ETransformState::ScaleConfirmRelease:
				Scenario.Transform.Progress.TransitionTo(ETransformState::VerifyTypedTransform);
				break;
			default:
				throw std::logic_error("Invalid transform text state");
		}
		return false;
	}
	return ExerciseTransformHistory(InEvents);
}

bool FEditorAcceptanceHarness::ExerciseTransformHistory(std::vector<FInputEvent>& InEvents)
{
	const auto CheckMatrix = [&](const FMat4& InExpected)
	{
		const auto Actual = Editor.Scene->FindNode(*Editor.Selection)->Local();
		for (unsigned Index = 0; Index < 16; ++Index)
		{
			if (std::abs(Actual.Values[Index] - InExpected.Values[Index]) > 1e-5f)
			{
				throw std::runtime_error("Live transform or history restored an incorrect value");
			}
		}
	};
	if (Scenario.Transform.Progress.Is(ETransformState::VerifyTypedTransform))
	{
		Scenario.ExerciseTransformResult = Editor.Scene->FindNode(*Editor.Selection)->Local();
		CheckMatrix(ComposeAffine({{2, 0, 0}, {0, 0, std::numbers::pi_v<float> / 4}, {1, 1.25f, 1}}));
		if (Editor.HistoryCursor != 3)
		{
			throw std::runtime_error("Transform gestures were not grouped per property");
		}
	}
	switch (Scenario.Transform.Progress.GetState())
	{
		case ETransformState::UndoScalePress:
		case ETransformState::UndoScaleRelease:
		case ETransformState::UndoRotationPress:
		case ETransformState::UndoRotationRelease:
		case ETransformState::UndoPositionPress:
		case ETransformState::UndoPositionRelease:
		case ETransformState::VerifyBaselineAndRedoPositionPress:
		case ETransformState::RedoPositionRelease:
		case ETransformState::RedoRotationPress:
		case ETransformState::RedoRotationRelease:
		case ETransformState::RedoScalePress:
		case ETransformState::RedoScaleRelease:
		case ETransformState::VerifyEditedTransformAndUndoScalePress:
		case ETransformState::SecondUndoScaleRelease:
		case ETransformState::SecondUndoRotationPress:
		case ETransformState::SecondUndoRotationRelease:
		case ETransformState::SecondUndoPositionPress:
		case ETransformState::SecondUndoPositionRelease:
		{
			const auto Command = DescribeTransformHistoryCommand(Scenario.Transform.Progress.GetState());
			SendTransformHistory(InEvents, Command);
			if (Scenario.Transform.Progress.Is(ETransformState::VerifyBaselineAndRedoPositionPress) ||
			    Scenario.Transform.Progress.Is(ETransformState::VerifyEditedTransformAndUndoScalePress))
			{
				const bool bEdited =
				    Scenario.Transform.Progress.Is(ETransformState::VerifyEditedTransformAndUndoScalePress);
				CheckMatrix(bEdited ? Scenario.ExerciseTransformResult : Scenario.ExerciseOriginal.Local());
				if (Editor.IsDirty() != bEdited)
				{
					throw std::runtime_error("Repeated Undo/Redo lost the document save point");
				}
			}
			Scenario.Transform.Progress.TransitionTo(Command.Destination);
			return false;
		}
		default:
			break;
	}
	if (Scenario.Transform.Progress.Is(ETransformState::VerifyRestoredTransform))
	{
		CheckMatrix(Scenario.ExerciseOriginal.Local());
		if (Editor.IsDirty() || Editor.HistoryCursor)
		{
			throw std::runtime_error("Repeated Ctrl+Z did not restore the initial document");
		}
	}
	if (!Scenario.Transform.Progress.IsAny(
	        {ETransformState::VerifyTypedTransform, ETransformState::VerifyRestoredTransform}))
	{
		return ExerciseUnchangedHistory(InEvents);
	}
	Scenario.Transform.Progress.TransitionTo(Scenario.Transform.Progress.Is(ETransformState::VerifyTypedTransform)
	                                             ? ETransformState::UndoScalePress
	                                             : ETransformState::FocusUnchangedValue);
	return false;
}

bool FEditorAcceptanceHarness::ExerciseUnchangedHistory(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Transform.Progress.GetState())
	{
		case ETransformState::FocusUnchangedValue:
			if (ExerciseClick(InEvents,
			                  Scenario.Bounds.Require(FPropertyKey{RecordType<FSceneTransform>().Id, "position/x"}),
			                  Scenario.Transform.Click))
			{
				Scenario.Transform.Progress.TransitionTo(ETransformState::SelectChangedValue);
			}
			return false;
		case ETransformState::SelectChangedValue:
			TypeValue(InEvents, ETransformTextAction::SelectAllPress, "2");
			Scenario.Transform.Progress.TransitionTo(ETransformState::TypeChangedValue);
			return false;
		case ETransformState::TypeChangedValue:
			TypeValue(InEvents, ETransformTextAction::TypeValue, "2");
			Scenario.Transform.Progress.TransitionTo(ETransformState::SelectOriginalValue);
			return false;
		case ETransformState::SelectOriginalValue:
			TypeValue(InEvents, ETransformTextAction::SelectAllPress, "0");
			Scenario.Transform.Progress.TransitionTo(ETransformState::TypeOriginalValue);
			return false;
		case ETransformState::TypeOriginalValue:
			TypeValue(InEvents, ETransformTextAction::TypeValue, "0");
			Scenario.Transform.Progress.TransitionTo(ETransformState::AwaitOriginalValueCommit);
			return false;
		case ETransformState::AwaitOriginalValueCommit:
			Scenario.Transform.Progress.TransitionTo(ETransformState::AwaitOriginalValueSettlement);
			return false;
		case ETransformState::AwaitOriginalValueSettlement:
			Scenario.Transform.Progress.TransitionTo(ETransformState::VerifyOriginalValueAndUndoPress);
			return false;
		default:
			break;
	}
	if (Scenario.Transform.Progress.Is(ETransformState::VerifyOriginalValueAndUndoPress) ||
	    Scenario.Transform.Progress.Is(ETransformState::VerifyOriginalValueAndUndoAgainPress))
	{
		if (!Editor.IsDirty() || Editor.HistoryCursor != 1 ||
		    Editor.Scene->FindNode(*Editor.Selection)->Local().Values != Scenario.ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Returning to the original value removed an editing transaction");
		}
	}
	if (Scenario.Transform.Progress.IsAny(
	        {ETransformState::VerifyOriginalValueAndUndoPress, ETransformState::UndoOriginalValueRelease,
	         ETransformState::VerifyUnchangedUndoAndRedoPress, ETransformState::RedoOriginalValueRelease,
	         ETransformState::VerifyOriginalValueAndUndoAgainPress, ETransformState::UndoOriginalValueAgainRelease}))
	{
		SendTransformHistory(InEvents, DescribeTransformHistoryCommand(Scenario.Transform.Progress.GetState()));
	}
	if (Scenario.Transform.Progress.Is(ETransformState::VerifyUnchangedUndoAndRedoPress) ||
	    Scenario.Transform.Progress.Is(ETransformState::BeginNumericDrag))
	{
		if (Editor.IsDirty() || Editor.HistoryCursor)
		{
			throw std::runtime_error("Undo of an unchanged-value gesture did not restore the save point");
		}
	}
	if (IsTransformDragState(Scenario.Transform.Progress.GetState()))
	{
		return ExerciseTransformDrag(InEvents);
	}
	Scenario.Transform.Progress.TransitionTo(
	    DescribeTransformHistoryCommand(Scenario.Transform.Progress.GetState()).Destination);
	return false;
}

bool FEditorAcceptanceHarness::ExerciseTransformDrag(std::vector<FInputEvent>& InEvents)
{
	const auto Phase = DescribeTransformDragContext(Scenario.Transform.Progress.GetState()).Action;
	if (Phase == ETransformDragAction::BeginDrag)
	{
		// A fast Release run must start a new gesture, not double-click the preceding text edit.
		if (!Scenario.TransformDragReadyAt)
		{
			Scenario.TransformDragReadyAt = ClockNanoseconds() + 500'000'000;
			FInputEvent Modifiers;
			Modifiers.Type = EEventType::Key;
			InEvents.push_back(Modifiers);
		}
		if (ClockNanoseconds() < Scenario.TransformDragReadyAt)
		{
			return false;
		}
	}
	if ((Phase == ETransformDragAction::BeginDrag || Phase == ETransformDragAction::MoveForward ||
	     Phase == ETransformDragAction::MoveBackward))
	{
		const auto Bounds = Scenario.Bounds.Require(FPropertyKey{RecordType<FSceneTransform>().Id, "position/x"});
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (Bounds.X + Bounds.Z) / 2 + (Phase == ETransformDragAction::BeginDrag     ? 0
		                                      : Phase == ETransformDragAction::MoveForward ? 40
		                                                                                   : 20);
		Move.Y = (Bounds.Y + Bounds.W) / 2;
		InEvents.push_back(Move);
	}
	if (Phase == ETransformDragAction::BeginDrag || Phase == ETransformDragAction::ReleaseDrag)
	{
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = Phase == ETransformDragAction::BeginDrag;
		InEvents.push_back(Button);
	}
	if (Phase == ETransformDragAction::VerifyDrag)
	{
		if (!Editor.IsDirty() || Editor.HistoryCursor != 1 || Editor.History.size() != 1 ||
		    Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Numeric drag did not create exactly one live undo transaction: history=" +
			                         std::to_string(Editor.HistoryCursor) + "/" +
			                         std::to_string(Editor.History.size()) +
			                         " text=" + std::to_string(Editor.Gui->IsEditingText()) +
			                         " interaction=" + std::to_string(Editor.InspectorInteraction));
		}
	}
	if (Phase == ETransformDragAction::PressUndo || Phase == ETransformDragAction::ReleaseUndo)
	{
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = EKey::Z;
		Key.Modifiers = InputModifiers::Control;
		Key.bDown = Phase == ETransformDragAction::PressUndo;
		InEvents.push_back(Key);
	}
	if (Phase == ETransformDragAction::VerifyUndo)
	{
		if (Editor.IsDirty() || Editor.HistoryCursor ||
		    Editor.Scene->FindNode(*Editor.Selection)->Local().Values != Scenario.ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Ctrl+Z did not restore the value before numeric dragging");
		}
		return true;
	}
	switch (Scenario.Transform.Progress.GetState())
	{
		case ETransformState::BeginNumericDrag:
			Scenario.Transform.Progress.TransitionTo(ETransformState::MoveNumericDragForward);
			break;
		case ETransformState::MoveNumericDragForward:
			Scenario.Transform.Progress.TransitionTo(ETransformState::MoveNumericDragBackward);
			break;
		case ETransformState::MoveNumericDragBackward:
			Scenario.Transform.Progress.TransitionTo(ETransformState::ReleaseNumericDrag);
			break;
		case ETransformState::ReleaseNumericDrag:
			Scenario.Transform.Progress.TransitionTo(ETransformState::VerifyNumericDrag);
			break;
		case ETransformState::VerifyNumericDrag:
			Scenario.Transform.Progress.TransitionTo(ETransformState::UndoNumericDragPress);
			break;
		case ETransformState::UndoNumericDragPress:
			Scenario.Transform.Progress.TransitionTo(ETransformState::UndoNumericDragRelease);
			break;
		case ETransformState::UndoNumericDragRelease:
			Scenario.Transform.Progress.TransitionTo(ETransformState::VerifyNumericDragUndo);
			break;
		default:
			throw std::logic_error("Invalid transform drag state");
	}
	return false;
}
} // namespace Hyperion
