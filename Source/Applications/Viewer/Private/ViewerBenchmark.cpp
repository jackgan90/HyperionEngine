#include "Hyperion/Core/Core.h"
#include "ViewerApplication.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <numeric>

namespace Hyperion
{
namespace
{
void WriteShadowBenchmark(std::ostream& InOutput, const FForwardPipelineStatistics& InPipeline,
                          const FDeviceStats& InDevice)
{
	double ShadowGpu{};
	double ForwardGpu{};
	std::array<double, 4> Cascades{};
	for (const auto& Pass : InDevice.GpuTiming.Passes)
	{
		if (Pass.Name.starts_with("Shadow cascade "))
		{
			ShadowGpu += Pass.Milliseconds;
			for (unsigned Index = 0; Index < Cascades.size(); ++Index)
			{
				if (Pass.Name.starts_with("Shadow cascade " + std::to_string(Index) + "/"))
				{
					Cascades[Index] += Pass.Milliseconds;
				}
			}
		}
		if (Pass.Name.starts_with("Forward/"))
		{
			ForwardGpu += Pass.Milliseconds;
		}
	}
	std::size_t Items{};
	std::size_t Draws{};
	std::size_t Failed{};
	std::uint64_t Uploads{};
	double ShadowMaterial{};
	double ShadowPlan{};
	double ShadowPrepare{};
	for (const auto& View : InPipeline.Views)
	{
		if (View.Usage == "ShadowDepth")
		{
			Items += View.Visibility.VisibleItems;
			Draws += View.Visibility.Draws;
			Failed += View.Visibility.Batches.FailedItems;
			Uploads += View.Visibility.Batches.UploadBytes;
			ShadowMaterial += View.Visibility.MaterialMilliseconds;
			ShadowPlan += View.Visibility.Batches.PlanningMilliseconds;
			ShadowPrepare += View.Visibility.Batches.PreparationMilliseconds;
		}
	}
	InOutput << ',' << InPipeline.bShadows << ',' << InPipeline.PreparationMilliseconds << ','
	         << InPipeline.ShadowSetupMilliseconds << ',' << Items << ',' << Draws << ',' << Failed << ',' << Uploads
	         << ',' << InPipeline.ShadowTextureBytes << ',' << InDevice.GpuTiming.Frame << ',' << ShadowGpu << ','
	         << ForwardGpu;
	for (const auto Time : Cascades)
	{
		InOutput << ',' << Time;
	}
	InOutput << ',' << InDevice.GpuAllocationBytes << ',' << InDevice.DescriptorAllocations << ','
	         << InDevice.PipelinesCreated << ',' << InDevice.ConstantBytesWritten << ',' << ShadowMaterial << ','
	         << ShadowPlan << ',' << ShadowPrepare;
}
} // namespace

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

void FViewerApplication::MatchBenchmarkTimings()
{
	if (BenchmarkGpu.DroppedFrames || BenchmarkGpu.Frames.empty())
	{
		throw std::runtime_error("Incomplete GPU timing capture");
	}
	auto& Timings = BenchmarkGpu.Frames;
	std::sort(Timings.begin(), Timings.end(),
	          [](const auto& InA, const auto& InB)
	          {
		          return InA.Frame < InB.Frame;
	          });
	for (std::size_t Index = 0; Index < Timings.size(); ++Index)
	{
		if (Timings[Index].Swapchain != Timings.front().Swapchain ||
		    (Index && Timings[Index].Frame == Timings[Index - 1].Frame))
		{
			throw std::runtime_error("Ambiguous GPU timing submission identity");
		}
	}
	for (auto& Frame : BenchmarkFrames)
	{
		const auto Expected = static_cast<std::uint64_t>(Frame.Frame) + 1;
		const auto Found = std::lower_bound(Timings.begin(), Timings.end(), Expected,
		                                    [](const auto& InTiming, std::uint64_t InFrame)
		                                    {
			                                    return InTiming.Frame < InFrame;
		                                    });
		if (Frame.Device.SubmittedFrames != Expected || Found == Timings.end() || Found->Frame != Expected)
		{
			throw std::runtime_error("Benchmark interval contains an untimed or unsubmitted frame");
		}
		Frame.Device.GpuTiming = std::move(*Found);
	}
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
	MatchBenchmarkTimings();
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
	Output
	    << ",shadows,pipeline_prepare_ms,shadow_setup_ms,shadow_items,shadow_draws,shadow_failed,shadow_upload_bytes,"
	       "shadow_payload_bytes,gpu_sample_frame,shadow_gpu_ms,forward_gpu_ms,cascade0_gpu_ms,cascade1_gpu_ms,"
	       "cascade2_gpu_ms,cascade3_gpu_ms,gpu_allocation_bytes,descriptor_allocations,pipelines_created,constant_"
	       "bytes_written,"
	       "shadow_material_ms,shadow_plan_ms,shadow_prepare_ms";
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
		WriteShadowBenchmark(Output, Frame.Pipeline, Frame.Device);
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
