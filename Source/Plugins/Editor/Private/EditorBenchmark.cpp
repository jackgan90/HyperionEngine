#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include <array>
#include <fstream>
#include <iomanip>

namespace Hyperion
{
void FEditorPlugin::BenchmarkCamera()
{
	if (ReadyFrames < Options.BenchmarkWarmup)
	{
		return;
	}
	if (Options.bBenchmarkCamera)
	{
		const auto Events = BenchmarkCameraInput(ReadyFrames - Options.BenchmarkWarmup, Options.BenchmarkCameraStep);
		FSceneCameraController BenchmarkNavigation(ESceneCameraNavigationMode::Orbit);
		BenchmarkNavigation.Input(ViewCamera, Events, false, false);
	}
	if (Options.bBenchmarkLight)
	{
		const auto Light = Scene->GetLightingSelection().Directional.Handle;
		FSceneNodeView View;
		if (!Light || !Scene->GetNodeView(*Light, View))
		{
			throw std::runtime_error("Light benchmark requires a main light");
		}
		auto Node = *View.Node;
		const auto Pose = ExtractScenePose(View.World);
		const auto Forward = ScaleVector(BenchmarkLightDirection(ReadyFrames - Options.BenchmarkWarmup), -1);
		const auto World = SceneCameraTransform(Pose.Eye, Add(Pose.Eye, Forward));
		FSceneNodeView Parent;
		const auto ParentWorld =
		    Scene->GetNodeView(Scene->FindHandle(Node.Parent()), Parent) ? Parent.World : Identity();
		Node.Local() = Multiply(Inverse(ParentWorld), World);
		// One shared-domain transaction for the isolated workload, restored before completion.
		SceneDocument.CommitEdits({{*Light, std::move(Node)}}, Scene->GetRevision(), 1);
		bBenchmarkLightEdit = true;
	}
}

void FEditorPlugin::RecordBenchmark()
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
	if (!bBenchmarkTiming)
	{
		return;
	}
	const auto Main = RenderStats.MainView();
	if (!bViewportVisible || !Main.VisibleItems || Main.Batches.FailedItems)
	{
		throw std::runtime_error("Editor benchmark requires a visible nonempty successfully rendered scene");
	}
	HYP_PERF_PLOT(Frame, SceneDraws, Main.Draws);
	BenchmarkFrame.Pipeline = RenderStats;
	BenchmarkFrame.Nodes = Scene->GetNodes().size();
	BenchmarkFrame.Frame = ReadyFrames;
	BenchmarkFrame.Draws = Main.Draws;
	BenchmarkFrame.VisibleItems = Main.VisibleItems;
	BenchmarkFrame.Batches = Main.Batches;
	BenchmarkFrame.CpuLatencyMilliseconds = BenchmarkFrame.Milliseconds;
	BenchmarkFrame.Viewport = ViewportSize;
	const auto Logical = Window->LogicalSize();
	const auto Pixels = Window->PixelSize();
	const auto Bounds = ViewportRegion.Bounds;
	BenchmarkFrame.CaptureViewport = {
	    Bounds.X * Pixels.Width / Logical.Width, Bounds.Y * Pixels.Height / Logical.Height,
	    (Bounds.Z - Bounds.X) * Pixels.Width / Logical.Width, (Bounds.W - Bounds.Y) * Pixels.Height / Logical.Height};
	BenchmarkFrame.LoadMilliseconds = LoadMilliseconds;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          BenchmarkFrame.Device = Device->Statistics();
	                          }));
	BenchmarkSamples.push_back(BenchmarkFrame);
}

void FEditorPlugin::SaveBenchmark()
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
	if (bBenchmarkLightEdit)
	{
		if (SceneDocument.GetState().HistoryCursor != 1)
		{
			throw std::runtime_error("Light benchmark must be one history transaction");
		}
		SceneDocument.FinishInteraction();
		SceneDocument.Undo();
		if (SceneDocument.IsDirty())
		{
			throw std::runtime_error("Light benchmark failed to restore authored state");
		}
		bBenchmarkLightEdit = false;
	}
	FGpuTimingCapture Capture;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Device->WaitIdle();
		                          Capture = Device->EndGpuTimingCapture();
	                          }));
	MatchBenchmarkTimings(BenchmarkSamples, std::move(Capture));
	WriteRenderBenchmark(Options.Benchmark, BenchmarkSamples);
	Log(ELogLevel::Info, "Editor benchmark: " + std::to_string(BenchmarkSamples.size()) + " ready frames; vsync=off");
}
} // namespace Hyperion
