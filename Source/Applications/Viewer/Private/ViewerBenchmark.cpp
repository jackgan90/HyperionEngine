#include "Hyperion/Core/Core.h"
#include "ViewerApplication.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <numeric>

namespace Hyperion
{
void FViewerApplication::ExerciseBenchmarkCamera(int InFrame)
{
	if (!Options.bBenchmarkCamera || InFrame < Options.BenchmarkWarmup)
	{
		return;
	}
	// A small orbit exercises View invalidation every frame while preserving the visible workload.
	const int Phase = InFrame % 40;
	std::array<FInputEvent, 3> Events;
	Events[0].Type = EEventType::MouseButton;
	Events[0].Button = 1;
	Events[0].bDown = true;
	Events[1].Type = EEventType::MouseMove;
	Events[1].X = float(Phase < 20 ? Phase : 40 - Phase) * .1f;
	Events[2].Type = EEventType::MouseButton;
	Events[2].Button = 1;
	Events[2].bDown = false;
	ScenePlugin->Input(Events, false, false);
}

void FViewerApplication::SaveBenchmark()
{
	if (Options.Benchmark.empty())
	{
		return;
	}
	if (BenchmarkFrames.empty())
	{
		throw std::runtime_error("Benchmark ended before collecting any samples");
	}
	if (!Options.Benchmark.parent_path().empty())
	{
		std::filesystem::create_directories(Options.Benchmark.parent_path());
	}
	std::ofstream Output(Options.Benchmark);
	Output.exceptions(std::ios::failbit | std::ios::badbit);
	Output << "frame,frame_ms,scene_draws,visible_items,instanced_items,instanced_draws,single_draws,failed_items,"
	          "reused_chunks,rebuilt_chunks,packed_bytes,instance_upload_bytes,gpu_reuses,compat_reuses,compat_builds,"
	          "plan_ms,prepare_ms";
	for (std::size_t Index = 0; Index < static_cast<std::size_t>(ERenderBatchFallback::Count); ++Index)
	{
		Output << ",fallback_" << GetRenderBatchFallbackName(static_cast<ERenderBatchFallback>(Index));
	}
	Output << '\n' << std::fixed << std::setprecision(6);
	std::vector<double> Times;
	Times.reserve(BenchmarkFrames.size());
	for (const auto& Frame : BenchmarkFrames)
	{
		Output << Frame.Frame << ',' << Frame.Milliseconds << ',' << Frame.Draws << ',' << Frame.VisibleItems << ','
		       << Frame.Batches.InstancedItems << ',' << Frame.Batches.InstancedDraws << ','
		       << Frame.Batches.SingleDraws << ',' << Frame.Batches.FailedItems << ',' << Frame.Batches.ReusedChunks
		       << ',' << Frame.Batches.RebuiltChunks << ',' << Frame.Batches.PackedBytes << ','
		       << Frame.Batches.UploadBytes << ',' << Frame.Batches.GpuReuses << ','
		       << Frame.Batches.CompatibilityReuses << ',' << Frame.Batches.CompatibilityBuilds << ','
		       << Frame.Batches.PlanningMilliseconds << ',' << Frame.Batches.PreparationMilliseconds;
		for (const auto Count : Frame.Batches.Fallbacks)
		{
			Output << ',' << Count;
		}
		Output << '\n';
		Times.push_back(Frame.Milliseconds);
	}
	Output.close();
	const double Mean = std::accumulate(Times.begin(), Times.end(), 0.0) / Times.size();
	std::sort(Times.begin(), Times.end());
	Log(ELogLevel::Info, "Benchmark: " + std::to_string(Times.size()) + " frames; mean=" + std::to_string(Mean) +
	                         " ms; p95=" + std::to_string(Times[(Times.size() - 1) * 95 / 100]) +
	                         " ms; fps=" + std::to_string(1000.0 / Mean) +
	                         "; vsync=" + (Settings.bVsync ? "on" : "off") + "; csv=" + Options.Benchmark.string());
}
} // namespace Hyperion
