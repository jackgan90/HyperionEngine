#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Math/AffineTransform.h"
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
void TypeValue(std::vector<FInputEvent>& InEvents, unsigned InPhase, const char* InText)
{
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Key = InPhase < 3 ? EKey::A : EKey::Enter;
	Key.bDown = InPhase == 1 || InPhase == 3;
	Key.Modifiers = InPhase == 1 ? InputModifiers::Control : InputModifiers::None;
	InEvents.push_back(Key);
	if (InPhase == 2)
	{
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = InText;
		InEvents.push_back(std::move(Text));
	}
}
} // namespace

bool FEditorAcceptanceHarness::ExerciseTransformInput(std::vector<FInputEvent>& InEvents)
{
	const auto Type = RecordType<FSceneTransform>().Id;
	if (Scenario.TransformExerciseStep < 15)
	{
		const unsigned Field = Scenario.TransformExerciseStep / 5;
		const unsigned Phase = Scenario.TransformExerciseStep % 5;
		const std::array Ids{"position/x", "rotation/z", "scale/y"};
		const std::array Values{"2", "45", "1.25"};
		if (Phase == 0)
		{
			const auto PreviousStep = Scenario.ExerciseStep;
			ExerciseClick(InEvents, Scenario.InspectionBounds.at(Type + "/" + Ids[Field]));
			if (PreviousStep != Scenario.ExerciseStep)
			{
				Scenario.ExerciseStep = PreviousStep;
				++Scenario.TransformExerciseStep;
			}
		}
		else
		{
			// ImGui may deliver the queued key release and text on separate frames.
			if (Phase == 3 && Scenario.ExerciseWait++ == 0)
			{
				return false;
			}
			if (Phase == 3 && (!Editor.IsDirty() || Editor.HistoryCursor != Field + 1))
			{
				throw std::runtime_error("Inspector input was not live before Enter: field=" + std::to_string(Field) +
				                         " history=" + std::to_string(Editor.HistoryCursor) + " active=" +
				                         std::to_string(Editor.InspectorInteraction) + " error=" + Editor.Error);
			}
			Scenario.ExerciseWait = 0;
			TypeValue(InEvents, Phase, Values[Field]);
			++Scenario.TransformExerciseStep;
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
	if (Scenario.TransformExerciseStep == 15)
	{
		Scenario.ExerciseTransformResult = Editor.Scene->FindNode(*Editor.Selection)->Local();
		CheckMatrix(ComposeAffine({{2, 0, 0}, {0, 0, std::numbers::pi_v<float> / 4}, {1, 1.25f, 1}}));
		if (Editor.HistoryCursor != 3)
		{
			throw std::runtime_error("Transform gestures were not grouped per property");
		}
	}
	if (Scenario.TransformExerciseStep >= 16 && Scenario.TransformExerciseStep < 34)
	{
		const unsigned Offset = Scenario.TransformExerciseStep - 16;
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = Offset >= 6 && Offset < 12 ? EKey::Y : EKey::Z;
		Key.Modifiers = InputModifiers::Control;
		Key.bDown = Offset % 2 == 0;
		InEvents.push_back(Key);
		if (Offset == 6 || Offset == 12)
		{
			CheckMatrix(Offset == 6 ? Scenario.ExerciseOriginal.Local() : Scenario.ExerciseTransformResult);
			if (Editor.IsDirty() != (Offset == 12))
			{
				throw std::runtime_error("Repeated Undo/Redo lost the document save point");
			}
		}
	}
	if (Scenario.TransformExerciseStep == 34)
	{
		CheckMatrix(Scenario.ExerciseOriginal.Local());
		if (Editor.IsDirty() || Editor.HistoryCursor)
		{
			throw std::runtime_error("Repeated Ctrl+Z did not restore the initial document");
		}
	}
	if (Scenario.TransformExerciseStep >= 35)
	{
		return ExerciseUnchangedHistory(InEvents);
	}
	++Scenario.TransformExerciseStep;
	return false;
}

bool FEditorAcceptanceHarness::ExerciseUnchangedHistory(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.TransformExerciseStep == 35)
	{
		const auto PreviousStep = Scenario.ExerciseStep;
		ExerciseClick(InEvents, Scenario.InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/x"));
		if (PreviousStep != Scenario.ExerciseStep)
		{
			Scenario.ExerciseStep = PreviousStep;
			++Scenario.TransformExerciseStep;
		}
		return false;
	}
	if (Scenario.TransformExerciseStep < 40)
	{
		const unsigned Phase = Scenario.TransformExerciseStep - 35;
		TypeValue(InEvents, Phase <= 2 ? Phase : Phase - 2, Phase <= 2 ? "2" : "0");
	}
	if (Scenario.TransformExerciseStep == 42 || Scenario.TransformExerciseStep == 46)
	{
		if (!Editor.IsDirty() || Editor.HistoryCursor != 1 ||
		    Editor.Scene->FindNode(*Editor.Selection)->Local().Values != Scenario.ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Returning to the original value removed an editing transaction");
		}
	}
	if (Scenario.TransformExerciseStep >= 42 && Scenario.TransformExerciseStep < 48)
	{
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = Scenario.TransformExerciseStep == 44 || Scenario.TransformExerciseStep == 45 ? EKey::Y : EKey::Z;
		Key.Modifiers = InputModifiers::Control;
		Key.bDown = Scenario.TransformExerciseStep % 2 == 0;
		InEvents.push_back(Key);
	}
	if (Scenario.TransformExerciseStep == 44 || Scenario.TransformExerciseStep == 48)
	{
		if (Editor.IsDirty() || Editor.HistoryCursor)
		{
			throw std::runtime_error("Undo of an unchanged-value gesture did not restore the save point");
		}
	}
	if (Scenario.TransformExerciseStep >= 48)
	{
		return ExerciseTransformDrag(InEvents);
	}
	++Scenario.TransformExerciseStep;
	return false;
}

bool FEditorAcceptanceHarness::ExerciseTransformDrag(std::vector<FInputEvent>& InEvents)
{
	const unsigned Phase = Scenario.TransformExerciseStep - 48;
	if (Phase == 0)
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
	if (Phase < 3)
	{
		const auto Bounds = Scenario.InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/x");
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (Bounds.X + Bounds.Z) / 2 + (Phase == 0 ? 0 : Phase == 1 ? 40 : 20);
		Move.Y = (Bounds.Y + Bounds.W) / 2;
		InEvents.push_back(Move);
	}
	if (Phase == 0 || Phase == 3)
	{
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = Phase == 0;
		InEvents.push_back(Button);
	}
	if (Phase == 4)
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
	if (Phase == 5 || Phase == 6)
	{
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = EKey::Z;
		Key.Modifiers = InputModifiers::Control;
		Key.bDown = Phase == 5;
		InEvents.push_back(Key);
	}
	if (Phase == 7)
	{
		if (Editor.IsDirty() || Editor.HistoryCursor ||
		    Editor.Scene->FindNode(*Editor.Selection)->Local().Values != Scenario.ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Ctrl+Z did not restore the value before numeric dragging");
		}
		return true;
	}
	++Scenario.TransformExerciseStep;
	return false;
}
} // namespace Hyperion
