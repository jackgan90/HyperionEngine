#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Reflection/Wire.h"

namespace Hyperion
{
namespace
{
struct FOptionAction
{
	const char* Control;
	const char* Selection;
};

constexpr std::array Actions{FOptionAction{"hud/visualizer", "0"},        FOptionAction{"hud/visualizer", "1"},
                             FOptionAction{"hud/visualizer", "2"},        FOptionAction{"hud/visualizer", "3"},
                             FOptionAction{"hud/visualizer", "4"},        FOptionAction{"hud/visualizer", "5"},
                             FOptionAction{"hud/visualizer", "6"},        FOptionAction{"render/gbuffer", "high"},
                             FOptionAction{"render/vsync", ""},           FOptionAction{"render/pipeline", "forward"},
                             FOptionAction{"render/vsync", ""},           FOptionAction{"render/pipeline", "deferred"},
                             FOptionAction{"render/gbuffer", "compact"},  FOptionAction{"render/vsync", ""},
                             FOptionAction{"render/pipeline", "forward"}, FOptionAction{"render/vsync", ""},
                             FOptionAction{"render/pipeline", "deferred"}};

std::string SettingsWire(const FRenderSettings& InSettings)
{
	return WriteJson(WriteRecordWire(RecordType<FRenderSettings>(), &InSettings));
}

FRenderSettings ExpectedSettings(FRenderSettings InBefore, FOptionAction InAction)
{
	const std::string_view Control(InAction.Control);
	if (Control == "hud/visualizer")
	{
		InBefore.DebugMode = ParseGBufferVisualizer(static_cast<std::uint32_t>(std::stoul(InAction.Selection)));
	}
	else if (Control == "render/pipeline")
	{
		InBefore.Pipeline = ParseSceneRenderPipeline(InAction.Selection);
	}
	else if (Control == "render/gbuffer")
	{
		InBefore.GBuffer = ParseGBufferPreset(InAction.Selection);
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
		Editor.bShowRenderSettings = Exercise.Case >= 7;
		Exercise.Before = Editor.Rendering;
		Exercise.Progress.TransitionTo(ERasterState::OpenChoice);
		return false;
	}
	if (Exercise.Progress.Is(ERasterState::OpenChoice) || Exercise.Progress.Is(ERasterState::SelectChoice))
	{
		const auto Key = Exercise.Progress.Is(ERasterState::OpenChoice)
		                     ? std::string(Action.Control)
		                     : std::string(Action.Control) + "/" + Action.Selection;
		if (ExerciseClick(InEvents, Scenario.InspectionBounds[Key], Exercise.Click))
		{
			Exercise.Progress.TransitionTo(Exercise.Progress.Is(ERasterState::OpenChoice) && Action.Selection[0]
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
		throw std::runtime_error("Raster GUI choice changed an unrelated field: " + std::string(Action.Control) + "/" +
		                         Action.Selection);
	}
	CheckRasterOptionFrame();
	Exercise.Progress.TransitionTo(ERasterState::PrepareChoice);
	++Exercise.Case;
	return false;
}
} // namespace Hyperion
