#include "EditorApplication.h"
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
	Key.Modifiers = InPhase == 1 ? 1 : 0;
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

bool FEditorApplication::ExerciseTransformInput(std::vector<FInputEvent>& InEvents)
{
	const auto Type = RecordType<FSceneTransform>().Id;
	if (TransformExerciseStep < 15)
	{
		const unsigned Field = TransformExerciseStep / 5;
		const unsigned Phase = TransformExerciseStep % 5;
		const std::array Ids{"position/x", "rotation/z", "scale/y"};
		const std::array Values{"2", "45", "1.25"};
		if (Phase == 0)
		{
			const auto PreviousStep = ExerciseStep;
			ExerciseClick(InEvents, InspectionBounds.at(Type + "/" + Ids[Field]));
			if (PreviousStep != ExerciseStep)
			{
				ExerciseStep = PreviousStep;
				++TransformExerciseStep;
			}
		}
		else
		{
			TypeValue(InEvents, Phase, Values[Field]);
			++TransformExerciseStep;
		}
		return false;
	}
	if (TransformExerciseStep == 15)
	{
		if (!HasDrafts() || Scene->FindNode(*Selection)->Local().Values != ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Transform GUI did not isolate the editing draft");
		}
		const auto PreviousStep = ExerciseStep;
		ExerciseClick(InEvents, InspectionBounds.at(Type + "/apply"));
		if (PreviousStep != ExerciseStep)
		{
			ExerciseStep = PreviousStep;
			++TransformExerciseStep;
		}
		return false;
	}
	if (TransformExerciseStep == 16)
	{
		ExerciseTransformResult = Scene->FindNode(*Selection)->Local();
		const auto Expected = ComposeAffine({{2, 0, 0}, {0, 0, std::numbers::pi_v<float> / 4}, {1, 1.25f, 1}});
		for (unsigned Index = 0; Index < 16; ++Index)
		{
			if (std::abs(ExerciseTransformResult.Values[Index] - Expected.Values[Index]) > 1e-5f)
			{
				throw std::runtime_error("Transform GUI axis input or degree mapping is incorrect");
			}
		}
		Undo();
		if (IsDirty() || Scene->FindNode(*Selection)->Local().Values != ExerciseOriginal.Local().Values)
		{
			throw std::runtime_error("Transform Undo did not restore the exact original matrix");
		}
		Redo();
		if (!IsDirty() || Scene->FindNode(*Selection)->Local().Values != ExerciseTransformResult.Values)
		{
			throw std::runtime_error("Transform Redo did not restore the edited matrix");
		}
		Undo();
		++TransformExerciseStep;
	}
	return true;
}
} // namespace Hyperion
