#include "EditorApplication.h"
#include "Hyperion/IO/Path.h"
#include <fstream>
#include <iomanip>

namespace Hyperion
{
void FEditorPlugin::WriteReport()
{
	if (Options.Report.empty())
	{
		return;
	}
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          DeviceStats = Device->Statistics();
	                          }));
	if (!Options.Report.parent_path().empty())
	{
		std::filesystem::create_directories(Options.Report.parent_path());
	}
	const auto Objects = PlacementRegistry.Search("All", {});
	const auto Unavailable =
	    std::count_if(Objects.begin(), Objects.end(),
	                  [&](const auto* InObject)
	                  {
		                  return GetPlacementPreparation(*InObject).State != EPlacementPreparationState::Ready;
	                  });
	std::ofstream Stream(Options.Report);
	Stream << std::boolalpha << "{\n"
	       << "\"scene\": " << std::quoted(CurrentPath) << ",\n"
	       << "\"asset_root\": " << std::quoted(PathToUtf8(Context.Require<FContentRootService>().Directory())) << ",\n"
	       << "\"content_verified\": " << Acceptance.bContentVerified << ",\n"
	       << "\"root_restore_failed\": " << !Context.Require<FContentRootService>().StartupError.empty() << ",\n"
	       << "\"open_count\": " << OpenCount << ",\n"
	       << "\"document_verified\": " << Acceptance.bDocumentVerified << ",\n"
	       << "\"views_verified\": " << Acceptance.bViewsVerified << ",\n"
	       << "\"render_controls_verified\": " << Acceptance.bRenderControlsVerified << ",\n"
	       << "\"gizmo_verified\": " << Acceptance.bGizmoVerified << ",\n"
	       << "\"multiselect_verified\": " << Acceptance.bMultiSelectionVerified << ",\n"
	       << "\"selection_shortcuts_verified\": " << Acceptance.bSelectionShortcutsVerified << ",\n"
	       << "\"clipboard_verified\": " << Acceptance.bClipboardVerified << ",\n"
	       << "\"framing_verified\": " << Acceptance.bFramingVerified << ",\n"
	       << "\"reparent_verified\": " << Acceptance.bReparentVerified << ",\n"
	       << "\"picking_verified\": " << Acceptance.bPickingVerified << ",\n"
	       << "\"outlines_verified\": " << Acceptance.bOutlinesVerified << ",\n"
	       << "\"placement_verified\": " << Acceptance.bPlacementVerified << ",\n"
	       << "\"model_placement_verified\": " << Acceptance.bModelPlacementVerified << ",\n"
	       << "\"placement_status\": " << std::quoted(PlacementStatus) << ",\n"
	       << "\"placement_unavailable\": " << Unavailable << ",\n"
	       << "\"save_ms\": " << LastSaveMilliseconds << ",\n"
	       << "\"document_dirty\": " << IsDirty() << ",\n"
	       << "\"ready_frames\": " << ReadyFrames << ",\n"
	       << "\"nodes\": " << Scene->GetNodes().size() << ",\n"
	       << "\"scene_error\": " << std::quoted(Scene->GetStatus().Error) << ",\n"
	       << "\"failed_models\": " << Scene->GetStatus().FailedModels << ",\n"
	       << "\"load_error_observed\": " << bLoadErrorObserved << ",\n"
	       << "\"draws\": " << RenderStats.MainView().Draws << ",\n"
	       << "\"validation_errors\": " << DeviceStats.ValidationErrors << ",\n"
	       << "\"viewport_width\": " << Viewport.ViewportSize.Width << ",\n"
	       << "\"viewport_height\": " << Viewport.ViewportSize.Height << ",\n"
	       << "\"exercise_step\": " << Acceptance.ExerciseStep << ",\n"
	       << "\"movement\": " << Acceptance.bMovementVerified << ",\n"
	       << "\"movement_gate\": " << Acceptance.bMovementGateVerified << ",\n"
	       << "\"right_release\": " << Acceptance.bRightReleaseVerified << ",\n"
	       << "\"look\": " << Acceptance.bLookVerified << ",\n"
	       << "\"dolly\": " << Acceptance.bDollyVerified << ",\n"
	       << "\"wheel_speed\": " << Acceptance.bSpeedVerified << ",\n"
	       << "\"movement_speed\": " << Camera.GetMovementSpeed(Viewport.ViewCamera) << ",\n"
	       << "\"input_isolation\": " << Acceptance.bInputIsolationVerified << "\n}\n";
	if (!Stream)
	{
		throw std::runtime_error("Could not write editor report");
	}
}
} // namespace Hyperion
