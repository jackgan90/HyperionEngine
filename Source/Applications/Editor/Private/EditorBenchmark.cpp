#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include <array>
#include <fstream>
#include <iomanip>

namespace Hyperion
{
void FEditorApplication::BenchmarkCamera()
{
	if (!Options.bBenchmarkCamera || ReadyFrames < Options.BenchmarkWarmup)
	{
		return;
	}
	const auto Frame = ReadyFrames - Options.BenchmarkWarmup;
	const auto Phase = Frame % 40;
	const auto Previous = (Frame + 39) % 40;
	std::array<FInputEvent, 3> Events;
	Events[0].Type = EEventType::MouseButton;
	Events[0].Button = 1;
	Events[0].bDown = true;
	Events[0].X = Frame == 0 ? 0.f : float(Previous < 20 ? Previous : 40 - Previous) * .1f;
	Events[1].Type = EEventType::MouseMove;
	Events[1].X = float(Phase < 20 ? Phase : 40 - Phase) * .1f;
	Events[2].Type = EEventType::MouseButton;
	Events[2].Button = 1;
	Events[2].X = Events[1].X;
	Camera.Input(ViewCamera, Events, false, false);
}

void FEditorApplication::RecordBenchmark()
{
	const auto& Status = Scene->GetStatus();
	if (!Status.Error.empty() || !Status.PublicationError.empty() || Status.FailedModels || Status.FailedSkies)
	{
		throw std::runtime_error("Editor benchmark scene failed: " + Status.Error + Status.PublicationError);
	}
	if (!Status.bReady)
	{
		if (ClockNanoseconds() - BenchmarkStarted > 120'000'000'000ull)
		{
			throw std::runtime_error("Editor benchmark scene readiness timed out");
		}
		return;
	}
	if (!LoadMilliseconds)
	{
		LoadMilliseconds = double(ClockNanoseconds() - BenchmarkStarted) / 1e6;
	}
	if (ReadyFrames < Options.BenchmarkWarmup)
	{
		return;
	}
	const auto Main = RenderStats.MainView();
	if (!bViewportVisible || !Main.VisibleItems || Main.Batches.FailedItems)
	{
		throw std::runtime_error("Editor benchmark requires a visible nonempty successfully rendered scene");
	}
	BenchmarkFrame.Pipeline = RenderStats;
	BenchmarkFrame.Nodes = Scene->GetNodes().size();
	BenchmarkSamples.push_back(BenchmarkFrame);
}

void FEditorApplication::SaveBenchmark()
{
	if (Options.Benchmark.empty())
	{
		return;
	}
	if (BenchmarkSamples.size() != Options.BenchmarkSamples)
	{
		throw std::runtime_error("Editor benchmark ended without the requested sample coverage");
	}
	if (!Options.Benchmark.parent_path().empty())
	{
		std::filesystem::create_directories(Options.Benchmark.parent_path());
	}
	std::ofstream Stream(Options.Benchmark);
	Stream << std::setprecision(10);
	Stream << "frame,frame_ms,scene_ms,gui_ms,render_wait_ms,preparation_ms,material_ms,nodes,visible_items,"
	          "scene_draws,failed_items,scene_target_bytes,shadow_bytes,viewport_width,viewport_height,load_ms\n";
	for (std::size_t Index = 0; Index < BenchmarkSamples.size(); ++Index)
	{
		const auto& Sample = BenchmarkSamples[Index];
		const auto Main = Sample.Pipeline.MainView();
		Stream << Index << ',' << Sample.FrameMilliseconds << ',' << Sample.SceneMilliseconds << ','
		       << Sample.GuiMilliseconds << ',' << Sample.RenderMilliseconds << ','
		       << Sample.Pipeline.PreparationMilliseconds << ',' << Main.MaterialMilliseconds << ',' << Sample.Nodes
		       << ',' << Main.VisibleItems << ',' << Main.Draws << ',' << Main.Batches.FailedItems << ','
		       << Sample.Pipeline.SceneTargetBytes << ',' << Sample.Pipeline.ShadowTextureBytes << ','
		       << ViewportSize.Width << ',' << ViewportSize.Height << ',' << LoadMilliseconds << '\n';
	}
	if (!Stream)
	{
		throw std::runtime_error("Could not write editor benchmark");
	}
	Log(ELogLevel::Info, "Editor benchmark: " + std::to_string(BenchmarkSamples.size()) + " ready frames; vsync=off");
}
} // namespace Hyperion
