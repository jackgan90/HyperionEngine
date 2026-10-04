#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Reflection/Wire.h"
#include <variant>

namespace Hyperion
{
namespace
{
struct FViewportChoiceAction
{
	std::uint32_t WireValue;
	std::variant<ESceneCullingMode, EOutlineOverlapMode> Identity;

	std::string_view Control() const
	{
		return std::holds_alternative<ESceneCullingMode>(Identity) ? "view/culling" : "outline/mode";
	}
};

constexpr std::array Actions{
    FViewportChoiceAction{0, ESceneCullingMode::None}, FViewportChoiceAction{1, ESceneCullingMode::Linear},
    FViewportChoiceAction{2, ESceneCullingMode::Bvh}, FViewportChoiceAction{1, EOutlineOverlapMode::PerObject},
    FViewportChoiceAction{0, EOutlineOverlapMode::Union}};

std::string OptionsWire(const FSceneViewportOptions& InOptions)
{
	return WriteJson(WriteRecordWire(RecordType<FSceneViewportOptions>(), &InOptions));
}
} // namespace

void FEditorAcceptanceHarness::CheckViewportChoice() const
{
	const auto& Exercise = Scenario.ViewportChoiceExercise;
	const auto& Action = Actions[Exercise.Case];
	auto Expected = Exercise.Before;
	bool bChoiceMatches{};
	if (const auto* Culling = std::get_if<ESceneCullingMode>(&Action.Identity))
	{
		Expected.Culling = Action.WireValue;
		bChoiceMatches = Editor.CullingMode == *Culling;
	}
	else
	{
		Expected.OutlineMode = Action.WireValue;
		bChoiceMatches = Editor.OutlineSettings.Overlap == std::get<EOutlineOverlapMode>(Action.Identity);
	}
	if (!bChoiceMatches || OptionsWire(Editor.ViewportState().Options) != OptionsWire(Expected) ||
	    Editor.Scene->GetRevision() != Exercise.SceneRevision || Editor.IsDirty() ||
	    Editor.HistoryCursor != Exercise.HistoryCursor || Editor.History.size() != Exercise.HistorySize ||
	    Editor.RenderSettingsRevision != Exercise.RenderRevision)
	{
		throw std::runtime_error("Viewport GUI choice changed its meaning or unrelated state: " +
		                         std::string(Action.Control()) + "/" + std::to_string(Action.WireValue));
	}
}

bool FEditorAcceptanceHarness::ExerciseViewportChoices(std::vector<FInputEvent>& InEvents)
{
	auto& Exercise = Scenario.ViewportChoiceExercise;
	if (Exercise.Case >= Actions.size())
	{
		Editor.SetViewportOptions(Exercise.Initial);
		Editor.Gui->ClosePopups();
		Log(ELogLevel::Info, "Viewport culling and outline GUI choices preserved wire meaning and document state");
		return true;
	}
	const auto& Action = Actions[Exercise.Case];
	if (Exercise.Step == 0)
	{
		Exercise.Before = Editor.ViewportState().Options;
		if (Exercise.Case == 0)
		{
			Exercise.Initial = Exercise.Before;
			Exercise.SceneRevision = Editor.Scene->GetRevision();
			Exercise.RenderRevision = Editor.RenderSettingsRevision;
			Exercise.HistoryCursor = Editor.HistoryCursor;
			Exercise.HistorySize = Editor.History.size();
		}
		Exercise.Step = 1;
		return false;
	}
	if (Exercise.Step == 1 || Exercise.Step == 2)
	{
		const bool bOpenPopup = Exercise.Step == 1 && !Editor.bViewOptionsOpen;
		const auto Key = bOpenPopup ? std::string("view/options")
		                            : std::string(Action.Control()) +
		                                  (Exercise.Step == 2 ? "/" + std::to_string(Action.WireValue) : "");
		const auto Step = Scenario.ExerciseStep;
		ExerciseClick(InEvents, Scenario.InspectionBounds.at(Key));
		if (Scenario.ExerciseStep != Step && !bOpenPopup)
		{
			++Exercise.Step;
		}
		Scenario.ExerciseStep = Step;
		return false;
	}
	if (++Exercise.Wait < 2)
	{
		return false;
	}
	CheckViewportChoice();
	Exercise.Wait = 0;
	Exercise.Step = 0;
	++Exercise.Case;
	return false;
}
} // namespace Hyperion
