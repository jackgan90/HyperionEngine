#include "EditorApplication.h"
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

bool FEditorPlugin::ExerciseTransformInput(std::vector<FInputEvent>& InEvents)
{
	const auto Type = RecordType<FSceneTransform>().Id;
	if (Acceptance.TransformExerciseStep < 15)
	{
		const unsigned Field = Acceptance.TransformExerciseStep / 5;
		const unsigned Phase = Acceptance.TransformExerciseStep % 5;
		const std::array Ids{"position/x", "rotation/z", "scale/y"};
		const std::array Values{"2", "45", "1.25"};
		if (Phase == 0)
		{
			const auto PreviousStep = Acceptance.ExerciseStep;
			ExerciseClick(InEvents, InspectionBounds.at(Type + "/" + Ids[Field]));
			if (PreviousStep != Acceptance.ExerciseStep)
			{
				Acceptance.ExerciseStep = PreviousStep;
				++Acceptance.TransformExerciseStep;
			}
		}
		else
		{
			// ImGui may deliver the queued key release and text on separate frames.
			if (Phase == 3 && Acceptance.ExerciseWait++ == 0)
			{
				return false;
			}
			if (Phase == 3 && (!IsDirty() || HistoryCursor != Field + 1))
			{
				throw std::runtime_error("Inspector input was not live before Enter: field=" + std::to_string(Field) +
				                         " history=" + std::to_string(HistoryCursor) +
				                         " active=" + std::to_string(InspectorInteraction) + " error=" + Error);
			}
			Acceptance.ExerciseWait = 0;
			TypeValue(InEvents, Phase, Values[Field]);
			++Acceptance.TransformExerciseStep;
		}
		return false;
	}
	return ExerciseTransformHistory(InEvents);
}

bool FEditorPlugin::ExerciseTransformHistory(std::vector<FInputEvent>& InEvents)
{
	const auto CheckMatrix = [&](const FMat4& InExpected)
	{
		const auto Actual = Scene->FindNode(*Selection)->Local();
		for (unsigned Index = 0; Index < 16; ++Index)
		{
			if (std::abs(Actual.Values[Index] - InExpected.Values[Index]) > 1e-5f)
			{
				throw std::runtime_error("Live transform or history restored an incorrect value");
			}
		}
	};
	if (Acceptance.TransformExerciseStep == 15)
	{
		Acceptance.ExerciseTransformResult = Scene->FindNode(*Selection)->Local();
		CheckMatrix(ComposeAffine({{2, 0, 0}, {0, 0, std::numbers::pi_v<float> / 4}, {1, 1.25f, 1}}));
		if (HistoryCursor != 3)
		{
			throw std::runtime_error("Transform gestures were not grouped per property");
		}
	}
	if (Acceptance.TransformExerciseStep >= 16 && Acceptance.TransformExerciseStep < 34)
	{
		const unsigned Offset = Acceptance.TransformExerciseStep - 16;
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = Offset >= 6 && Offset < 12 ? EKey::Y : EKey::Z;
		Key.Modifiers = InputModifiers::Control;
		Key.bDown = Offset % 2 == 0;
		InEvents.push_back(Key);
		if (Offset == 6 || Offset == 12)
		{
			CheckMatrix(Offset == 6 ? Acceptance.ExerciseOriginal.Local() : Acceptance.ExerciseTransformResult);
			if (IsDirty() != (Offset == 12))
			{
				throw std::runtime_error("Repeated Undo/Redo lost the document save point");
			}
		}
	}
	if (Acceptance.TransformExerciseStep == 34)
	{
		CheckMatrix(Acceptance.ExerciseOriginal.Local());
		if (IsDirty() || HistoryCursor)
		{
			throw std::runtime_error("Repeated Ctrl+Z did not restore the initial document");
		}
	}
	if (Acceptance.TransformExerciseStep >= 35)
	{
		return ExerciseUnchangedHistory(InEvents);
	}
	++Acceptance.TransformExerciseStep;
	return false;
}

bool FEditorPlugin::ExerciseUnchangedHistory(std::vector<FInputEvent>& InEvents)
{
	if (Acceptance.TransformExerciseStep == 35)
	{
		const auto PreviousStep = Acceptance.ExerciseStep;
		ExerciseClick(InEvents, InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/x"));
		if (PreviousStep != Acceptance.ExerciseStep)
		{
			Acceptance.ExerciseStep = PreviousStep;
			++Acceptance.TransformExerciseStep;
		}
		return false;
	}
	if (Acceptance.TransformExerciseStep < 40)
	{
		const unsigned Phase = Acceptance.TransformExerciseStep - 35;
		TypeValue(InEvents, Phase <= 2 ? Phase : Phase - 2, Phase <= 2 ? "2" : "0");
	}
	if (Acceptance.TransformExerciseStep == 42 || Acceptance.TransformExerciseStep == 46)
	{
		if (!IsDirty() || HistoryCursor != 1 ||
		    Scene->FindNode(*Selection)->Local().Values != Acceptance.ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Returning to the original value removed an editing transaction");
		}
	}
	if (Acceptance.TransformExerciseStep >= 42 && Acceptance.TransformExerciseStep < 48)
	{
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = Acceptance.TransformExerciseStep == 44 || Acceptance.TransformExerciseStep == 45 ? EKey::Y : EKey::Z;
		Key.Modifiers = InputModifiers::Control;
		Key.bDown = Acceptance.TransformExerciseStep % 2 == 0;
		InEvents.push_back(Key);
	}
	if (Acceptance.TransformExerciseStep == 44 || Acceptance.TransformExerciseStep == 48)
	{
		if (IsDirty() || HistoryCursor)
		{
			throw std::runtime_error("Undo of an unchanged-value gesture did not restore the save point");
		}
	}
	if (Acceptance.TransformExerciseStep >= 48)
	{
		return ExerciseTransformDrag(InEvents);
	}
	++Acceptance.TransformExerciseStep;
	return false;
}

bool FEditorPlugin::ExerciseTransformDrag(std::vector<FInputEvent>& InEvents)
{
	const unsigned Phase = Acceptance.TransformExerciseStep - 48;
	if (Phase == 0)
	{
		// A fast Release run must start a new gesture, not double-click the preceding text edit.
		if (!Acceptance.TransformDragReadyAt)
		{
			Acceptance.TransformDragReadyAt = ClockNanoseconds() + 500'000'000;
			FInputEvent Modifiers;
			Modifiers.Type = EEventType::Key;
			InEvents.push_back(Modifiers);
		}
		if (ClockNanoseconds() < Acceptance.TransformDragReadyAt)
		{
			return false;
		}
	}
	if (Phase < 3)
	{
		const auto Bounds = InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/x");
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
		if (!IsDirty() || HistoryCursor != 1 || History.size() != 1 ||
		    Scene->FindNode(*Selection)->Local().Values == Acceptance.ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Numeric drag did not create exactly one live undo transaction: history=" +
			                         std::to_string(HistoryCursor) + "/" + std::to_string(History.size()) +
			                         " text=" + std::to_string(Gui->IsEditingText()) +
			                         " interaction=" + std::to_string(InspectorInteraction));
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
		if (IsDirty() || HistoryCursor ||
		    Scene->FindNode(*Selection)->Local().Values != Acceptance.ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Ctrl+Z did not restore the value before numeric dragging");
		}
		return true;
	}
	++Acceptance.TransformExerciseStep;
	return false;
}
} // namespace Hyperion
