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

	EEditorWidget Control() const
	{
		return std::holds_alternative<ESceneCullingMode>(Identity) ? EEditorWidget::CullingMode
		                                                           : EEditorWidget::OutlineMode;
	}

	FWidgetKey Selection() const
	{
		const auto Widget = std::holds_alternative<ESceneCullingMode>(Identity) ? EEditorWidget::CullingModeItem
		                                                                        : EEditorWidget::OutlineModeItem;
		return FWidgetKey::WithValue(Widget, WireValue);
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
		                         std::to_string(Exercise.Case) + " wire value " + std::to_string(Action.WireValue));
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
	if (Exercise.Progress.Is(EViewportChoiceState::OpenOptions))
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
		Exercise.Progress.TransitionTo(EViewportChoiceState::OpenChoice);
		return false;
	}
	if (Exercise.Progress.Is(EViewportChoiceState::OpenChoice) ||
	    Exercise.Progress.Is(EViewportChoiceState::SelectChoice))
	{
		const bool bOpenPopup = Exercise.Progress.Is(EViewportChoiceState::OpenChoice) && !Editor.bViewOptionsOpen;
		const auto Key = bOpenPopup ? FWidgetKey(EEditorWidget::ViewOptions)
		                 : Exercise.Progress.Is(EViewportChoiceState::SelectChoice) ? Action.Selection()
		                                                                            : FWidgetKey(Action.Control());
		if (ExerciseClick(InEvents, Scenario.Bounds.Require(Key), Exercise.Click) && !bOpenPopup)
		{
			Exercise.Progress.TransitionTo(Exercise.Progress.Is(EViewportChoiceState::OpenChoice)
			                                   ? EViewportChoiceState::SelectChoice
			                                   : EViewportChoiceState::VerifyChoice);
		}
		return false;
	}
	if (!Exercise.ChoiceObservation.Advance())
	{
		return false;
	}
	CheckViewportChoice();
	Exercise.ChoiceObservation.Restart();
	Exercise.Progress.TransitionTo(EViewportChoiceState::OpenOptions);
	++Exercise.Case;
	return false;
}
} // namespace Hyperion
