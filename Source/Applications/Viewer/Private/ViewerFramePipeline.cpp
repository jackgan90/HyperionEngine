#include "Hyperion/Core/Core.h"
#include "ViewerApplication.h"
#include "ViewerCaptureScope.h"

namespace Hyperion
{
std::function<void()> FViewerApplication::PrepareFrame(FViewerFrameInput InInput,
                                                       std::shared_ptr<FViewerFrameResult> InResult)
{
	// Only stable service pointers and Render-owned pipeline/plugin APIs are accessed here.
	// ReleaseGraphics drains every frame before replacing or destroying these services.
	auto Graph = BuildRenderGraph(InInput.Frame, InInput.Gui, std::move(InInput.Material), InInput.Shadows);
	auto Prepared = ScenePipeline->GetFrame();
	return [Graph = std::move(Graph), Prepared = std::move(Prepared), Result = std::move(InResult),
	        Tasks = &Services->Tasks, Swapchain = Swapchain.get(), Device = Device.get(), Size = InInput.Frame.Size,
	        bVsync = InInput.Frame.Settings.bVsync, bTakeCapture = InInput.bTakeCapture
#if HYP_ENABLE_RENDERDOC
	        ,
	        Capture = FrameCapture, Surface = InInput.Surface, bCaptureRdc = InInput.bCaptureRdc,
	        bAutoOpen = InInput.Frame.Settings.bRenderDocAutoOpen, FrameId = InInput.FrameId
#endif
	]() mutable
	{
#if HYP_ENABLE_RENDERDOC
		if (Capture && bCaptureRdc)
		{
			if (!Capture->RequestCapture())
			{
				throw std::runtime_error("Requested RDC capture was not produced: " + Capture->Status().Message);
			}
			Log(ELogLevel::Info, "RenderDoc target CPU frame: " + std::to_string(FrameId));
		}
		FFrameCaptureScope CaptureScope(*Tasks, Capture, Surface);
#endif
		Result->Screenshot = ExecuteGraphOnRhi(std::move(Graph), *Tasks, *Swapchain, Size, bVsync, bTakeCapture);
		Result->Device = Device->Statistics();
		Result->Pipeline = Prepared.Statistics();
#if HYP_ENABLE_RENDERDOC
		if (CaptureScope.Finish() && bAutoOpen)
		{
			// Open this capture before a later queued frame can replace LastCapture.
			Capture->OpenLastCapture();
		}
#endif
		Result->CompletedAt = ClockNanoseconds();
	};
}

void FViewerApplication::RenderFrame(int InFrame, FSize InSize, FGuiDrawData InGuiData, bool bInTakeCapture,
                                     bool bInCaptureRdc)
{
	FViewerFrameInput Input;
	Input.Frame = UpdateScene(InSize);
	Input.FrameId = static_cast<std::uint64_t>(InFrame) + 1;
	Input.Gui = std::move(InGuiData);
	Input.Material = RenderSession->FreezeFrame(float(ClockNanoseconds() / 1000000000.0));
	Input.Shadows = ShadowSettings;
	Input.Surface = Window->Surface();
	Input.bTakeCapture = bInTakeCapture;
	Input.bCaptureRdc = bInCaptureRdc;
	FPendingViewerFrame Pending;
	Pending.Result = std::make_shared<FViewerFrameResult>();
	Pending.Settings = Input.Frame.Settings;
	Pending.Frame = InFrame;
	Pending.StartedAt = FrameStartedAt;
	Pending.bTakeCapture = bInTakeCapture;
	Pending.bCaptureRdc = bInCaptureRdc;
	Pending.bBenchmark = !Options.Benchmark.empty() && InFrame >= Options.BenchmarkWarmup;
	if (!(ModelPlugin ? ModelPlugin->Ready() : ScenePlugin && ScenePlugin->Ready()))
	{
		Pending.SceneError = ModelPlugin   ? ModelPlugin->Status()
		                     : ScenePlugin ? ScenePlugin->Status()
		                                   : "No model plugin active";
	}
	if (Pending.bBenchmark && (ScenePlugin || ModelPlugin) && !Pending.SceneError.empty())
	{
		throw std::runtime_error("Benchmark scene is not ready; increase --benchmark-warmup and --frames");
	}
	// Retain the result before dispatch, so allocation failures cannot orphan admitted work.
	PendingFrames.push_back(std::move(Pending));
	try
	{
		PendingFrames.back().Ticket = FramePipeline->Submit(
		    [this, Input = std::move(Input), Result = PendingFrames.back().Result]() mutable
		    {
			    return PrepareFrame(std::move(Input), std::move(Result));
		    });
	}
	catch (...)
	{
		PendingFrames.pop_back();
		throw;
	}
	PendingCaptures += bInCaptureRdc;
}

void FViewerApplication::CollectFrames()
{
	while (!PendingFrames.empty() && PendingFrames.front().Ticket.Ready())
	{
		PendingFrames.front().Ticket.Wait();
		auto Pending = std::move(PendingFrames.front());
		PendingFrames.pop_front();
		PendingCaptures -= Pending.bCaptureRdc;
		auto& Result = *Pending.Result;
		Metrics.Device = Result.Device;
		Metrics.ResultFrame = Pending.Ticket.Frame();
		PipelineStatistics = std::move(Result.Pipeline);
		SceneStatistics = PipelineStatistics.MainView();
		Metrics.LegacyDisplayItems = 0;
		Metrics.SceneTargetBytes = PipelineStatistics.SceneTargetBytes;
		for (const auto& View : PipelineStatistics.Views)
		{
			if (View.Usage == "Forward")
			{
				Metrics.LegacyDisplayItems += View.Visibility.VisibleItems;
			}
		}
		SceneStatistics.UpdateMilliseconds = PipelineStatistics.Spatial.UpdateMilliseconds;
		SceneStatistics.IndexRebuilds = PipelineStatistics.Spatial.IndexRebuilds;
		SceneStatistics.IndexRefits = PipelineStatistics.Spatial.IndexRefits;
		HYP_PERF_PLOT(Frame, SceneDraws, double(SceneStatistics.Draws));
		if (Metrics.Device.ValidationErrors)
		{
			throw std::runtime_error("RHI validation errors");
		}
		if (Pending.bTakeCapture)
		{
			SaveScreenshot(std::move(Result.Screenshot), Pending.Settings, Pending.SceneError);
		}
		if (Pending.bBenchmark)
		{
			BenchmarkFrames.push_back({Pending.Frame, Pending.MainMilliseconds, SceneStatistics.Draws,
			                           SceneStatistics.VisibleItems, SceneStatistics.Batches, PipelineStatistics,
			                           Metrics.Device, double(Result.CompletedAt - Pending.StartedAt) / 1e6});
		}
	}
}

void FViewerApplication::DrainFrames()
{
	if (FramePipeline)
	{
		FramePipeline->Drain();
		CollectFrames();
		Metrics.FramePipeline = FramePipeline->Progress();
	}
}
} // namespace Hyperion
