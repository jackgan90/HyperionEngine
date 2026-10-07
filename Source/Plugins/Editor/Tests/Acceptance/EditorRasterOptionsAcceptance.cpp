#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Reflection/Wire.h"

namespace Hyperion
{
namespace
{
struct FToggleVsync
{
};

struct FOptionAction
{
	std::variant<EGBufferVisualizer, EGBufferPreset, ESceneRenderPipeline, FToggleVsync> Identity;

	EEditorWidget Control() const
	{
		if (std::holds_alternative<EGBufferVisualizer>(Identity))
		{
			return EEditorWidget::Visualizer;
		}
		if (std::holds_alternative<EGBufferPreset>(Identity))
		{
			return EEditorWidget::GBufferFormat;
		}
		return std::holds_alternative<ESceneRenderPipeline>(Identity) ? EEditorWidget::RenderPipeline
		                                                              : EEditorWidget::Vsync;
	}

	std::optional<FWidgetKey> Selection() const
	{
		if (const auto* Mode = std::get_if<EGBufferVisualizer>(&Identity))
		{
			return FWidgetKey::WithValue(EEditorWidget::VisualizerItem, ToVisualizerWireValue(*Mode));
		}
		if (const auto* Preset = std::get_if<EGBufferPreset>(&Identity))
		{
			return FWidgetKey::WithId(EEditorWidget::GBufferFormatItem, ToGBufferPresetToken(*Preset));
		}
		if (const auto* Pipeline = std::get_if<ESceneRenderPipeline>(&Identity))
		{
			return FWidgetKey::WithId(EEditorWidget::RenderPipelineItem, ToSceneRenderPipelineToken(*Pipeline));
		}
		return std::nullopt;
	}
};

constexpr std::array Actions{FOptionAction{EGBufferVisualizer::Lit},
                             FOptionAction{EGBufferVisualizer::BaseColor},
                             FOptionAction{EGBufferVisualizer::ShadingNormal},
                             FOptionAction{EGBufferVisualizer::MaterialChannels},
                             FOptionAction{EGBufferVisualizer::Emissive},
                             FOptionAction{EGBufferVisualizer::SceneDepth},
                             FOptionAction{EGBufferVisualizer::GeometryNormal},
                             FOptionAction{EGBufferPreset::HighPrecision},
                             FOptionAction{FToggleVsync{}},
                             FOptionAction{ESceneRenderPipeline::Forward},
                             FOptionAction{FToggleVsync{}},
                             FOptionAction{ESceneRenderPipeline::Deferred},
                             FOptionAction{EGBufferPreset::Compact},
                             FOptionAction{FToggleVsync{}},
                             FOptionAction{ESceneRenderPipeline::Forward},
                             FOptionAction{FToggleVsync{}},
                             FOptionAction{ESceneRenderPipeline::Deferred}};

std::string SettingsWire(const FRenderSettings& InSettings)
{
	return WriteJson(WriteRecordWire(RecordType<FRenderSettings>(), &InSettings));
}

FRenderSettings ExpectedSettings(FRenderSettings InBefore, FOptionAction InAction)
{
	if (const auto* Mode = std::get_if<EGBufferVisualizer>(&InAction.Identity))
	{
		InBefore.DebugMode = *Mode;
	}
	else if (const auto* Pipeline = std::get_if<ESceneRenderPipeline>(&InAction.Identity))
	{
		InBefore.Pipeline = *Pipeline;
	}
	else if (const auto* Preset = std::get_if<EGBufferPreset>(&InAction.Identity))
	{
		InBefore.GBuffer = *Preset;
	}
	else
	{
		InBefore.bVsync = !InBefore.bVsync;
	}
	return InBefore;
}
} // namespace

void FEditorAcceptanceHarness::CheckRasterOptionFrame() const
{
	FScenePipelineSettings Actual;
	Editor.Tasks.Wait(Editor.Tasks.Dispatch({EDomain::Render},
	                                        [&]
	                                        {
		                                        Actual = Editor.Pipeline->Configuration();
	                                        }));
	const auto Expected = MakePipelineSettings(Editor.Rendering);
	if (Actual.Pipeline != Expected.Pipeline || Actual.GBuffer != Expected.GBuffer ||
	    Actual.DebugMode != Expected.DebugMode || Actual.Exposure != Expected.Exposure ||
	    Actual.bClusteredLighting != Expected.bClusteredLighting ||
	    Editor.Scene->GetRevision() != Scenario.RasterOptionExercise.SceneRevision ||
	    Editor.HistoryCursor != Scenario.RasterOptionExercise.History || Editor.IsDirty())
	{
		throw std::runtime_error("Raster option GUI did not reach the main frame or changed scene history");
	}
}

bool FEditorAcceptanceHarness::ExerciseRasterOptions(std::vector<FInputEvent>& InEvents)
{
	auto& Exercise = Scenario.RasterOptionExercise;
	if (Exercise.Case > Actions.size())
	{
		return true;
	}
	if (Exercise.Case == Actions.size())
	{
		const auto Path = Editor.Options.ExerciseRenderControls / "RasterSettings.json";
		Editor.SaveRenderSettings(Path);
		if (SettingsWire(LoadRenderSettings(Path)) != SettingsWire(Editor.Rendering))
		{
			throw std::runtime_error("GUI raster choices did not survive settings save and reload");
		}
		Editor.SetRenderSettings(Editor.RenderSettingsRevision, Exercise.Initial);
		Editor.bShowRenderSettings = true;
		++Exercise.Case;
		Log(ELogLevel::Info,
		    "Raster GUI choices, isolated VSync edits, latent Forward selection and persistence passed");
		return true;
	}
	const auto Action = Actions[Exercise.Case];
	if (Exercise.Progress.Is(ERasterState::PrepareChoice))
	{
		if (Exercise.Case == 0)
		{
			Exercise.Initial = Editor.Rendering;
			Exercise.SceneRevision = Editor.Scene->GetRevision();
			Exercise.History = Editor.HistoryCursor;
			auto Initial = Editor.Rendering;
			Initial.Pipeline = ESceneRenderPipeline::Deferred;
			Initial.GBuffer = EGBufferPreset::Compact;
			Editor.SetRenderSettings(Editor.RenderSettingsRevision, Initial);
		}
		Editor.bShowRenderSettings = !std::holds_alternative<EGBufferVisualizer>(Action.Identity);
		Exercise.Before = Editor.Rendering;
		Exercise.Progress.TransitionTo(ERasterState::OpenChoice);
		return false;
	}
	if (Exercise.Progress.Is(ERasterState::OpenChoice) || Exercise.Progress.Is(ERasterState::SelectChoice))
	{
		const auto Selection = Action.Selection();
		const auto Key =
		    Exercise.Progress.Is(ERasterState::OpenChoice) ? FWidgetKey(Action.Control()) : Selection.value();
		if (ExerciseClick(InEvents, Scenario.Bounds.FindOrEmpty(Key), Exercise.Click))
		{
			Exercise.Progress.TransitionTo(Exercise.Progress.Is(ERasterState::OpenChoice) && Selection.has_value()
			                                   ? ERasterState::SelectChoice
			                                   : ERasterState::VerifyChoice);
		}
		return false;
	}
	if (!Exercise.ChoiceObservation.Advance())
	{
		return false;
	}
	Exercise.ChoiceObservation.Restart();
	if (SettingsWire(Editor.Rendering) != SettingsWire(ExpectedSettings(Exercise.Before, Action)))
	{
		throw std::runtime_error("Raster GUI choice changed an unrelated field at action " +
		                         std::to_string(Exercise.Case));
	}
	CheckRasterOptionFrame();
	Exercise.Progress.TransitionTo(ERasterState::PrepareChoice);
	++Exercise.Case;
	return false;
}
} // namespace Hyperion
