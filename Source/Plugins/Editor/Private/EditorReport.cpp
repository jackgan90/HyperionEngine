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
	const auto Objects = PlacementRegistry.Search(std::nullopt, {});
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
	       << "\"root_restore_failed\": " << !Context.Require<FContentRootService>().StartupError.empty() << ",\n"
	       << "\"open_count\": " << OpenCount << ",\n"
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
	       << "\"movement_speed\": " << Camera.GetMovementSpeed(Viewport.ViewCamera) << ",\n";
	Acceptance.WriteReport(Stream);
	Stream << "\n}\n";
	if (!Stream)
	{
		throw std::runtime_error("Could not write editor report");
	}
}
} // namespace Hyperion
