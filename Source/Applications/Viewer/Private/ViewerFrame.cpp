#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include "ViewerApplication.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace Hyperion
{
void FViewerApplication::RunFrames()
{
	if (Options.bBenchmarkCamera && !ScenePlugin)
	{
		throw std::invalid_argument("--benchmark-camera requires scene-viewer");
	}
	auto LastFrame = ClockNanoseconds();
	if (Options.bExerciseRdcUi && (!Gui || !Settings.bShowGui))
	{
		throw std::runtime_error("RDC UI exercise requires a visible diagnostics panel");
	}
	for (int Frame = 0; !Window->ShouldClose() && (!Options.Frames || Frame < Options.Frames); ++Frame)
	{
		UpdateProfiling(Frame);
		HYP_PERF_SCOPE_NAMED(EProfileCategory::Frame, "ApplicationFrame", FrameScope);
		HYP_PERF_VALUE(FrameScope, Frame);
		const auto Now = ClockNanoseconds();
		const float Delta = Frame ? float(Now - LastFrame) / 1e9f : 1.f / 60.f;
		LastFrame = Now;
		FrameStartedAt = Now;
		ExerciseBenchmarkCamera(Frame);
		Tick(Frame, Delta);
		if (!PendingFrames.empty() && PendingFrames.back().Frame == Frame)
		{
			PendingFrames.back().MainMilliseconds = double(ClockNanoseconds() - Now) / 1e6;
		}
		CollectFrames();
	}
	DrainFrames();
	if (Options.ProfileFrames)
	{
		SetProfilingMask(0);
	}
}

void FViewerApplication::PollInput()
{
	HYP_PERF_SCOPE_C(Frame, PollInput);
	Window->Poll();
	Services->Tasks.PumpMain();
	CollectFrames();
	Metrics.FramePipeline = FramePipeline->Progress();
	if (ModelPlugin || ScenePlugin)
	{
		Metrics.AssetStatus = ModelPlugin ? ModelPlugin->Status() : ScenePlugin->Status();
		Metrics.bSceneViewer = ScenePlugin != nullptr;
		for (const auto& Event : Window->Events())
		{
			if (Event.Type == EEventType::Key && Event.Key == EKey::Tab && Event.bDown)
			{
				Settings.bShowGui = !Settings.bShowGui;
			}
		}
	}
}

void FViewerApplication::ExerciseWindow(int InFrame)
{
	if (!Options.bExercise)
	{
		return;
	}
	if (InFrame == 2)
	{
		Window->Resize({960, 540});
	}
	if (InFrame == 4)
	{
		Window->Minimize();
	}
	if (InFrame == 6)
	{
		Window->Restore();
	}
}

FDebugActions FViewerApplication::BuildGui(int InFrame, float InDelta, FSize InLogical, FSize InPixels,
                                           FGuiDrawData& OutData)
{
	HYP_PERF_SCOPE_C(Frame, BuildGui);
	UpdateCaptureStatus();
	FDebugActions Actions;
	ShadowSettings.PreviewViewport.reset();
	if (Gui && Settings.bShowGui)
	{
		std::vector<FInputEvent> UiEvents(Window->Events().begin(), Window->Events().end());
		ExerciseCaptureInput(std::binary_search(Options.RdcFrames.begin(), Options.RdcFrames.end(), InFrame + 1),
		                     UiEvents);
		Gui->BeginFrame(InLogical, InPixels, InDelta, UiEvents);
		Actions = DrawDebugPanel(*Gui, Settings, Metrics, InLogical);
		RdcButtonBounds = Actions.CaptureRdcBounds;
		if (ScenePlugin)
		{
			ScenePlugin->DrawGui(*Gui, SceneStatistics, Options.bNoInstanceBatching);
			DrawShadowGui();
			if (ShadowSettings.PreviewViewport)
			{
				auto& Preview = *ShadowSettings.PreviewViewport;
				const float ScaleX = float(InPixels.Width) / InLogical.Width;
				const float ScaleY = float(InPixels.Height) / InLogical.Height;
				Preview.X *= ScaleX;
				Preview.Y *= ScaleY;
				Preview.Width *= ScaleX;
				Preview.Height *= ScaleY;
			}
		}
		OutData = Gui->Render();
	}
	return Actions;
}

void FViewerApplication::Tick(int InFrame, float InDelta)
{
	PollInput();
	ExerciseWindow(InFrame);
	const auto Size = Window->PixelSize();
	const auto Logical = Window->LogicalSize();
	if (Window->Minimized() || !Size.Width || !Size.Height)
	{
		UpdateScene(Size);
		FramePipeline->Skip();
		if (!Options.Benchmark.empty() && InFrame >= Options.BenchmarkWarmup)
		{
			throw std::runtime_error("Benchmark tick did not render a frame");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return;
	}
	Metrics.Threads = Services->Tasks.Statistics();
	Metrics.FrameMilliseconds.push_back(InDelta * 1000);
	if (Metrics.FrameMilliseconds.size() > 120)
	{
		Metrics.FrameMilliseconds.erase(Metrics.FrameMilliseconds.begin());
	}
	FGuiDrawData GuiData;
	const auto Actions = BuildGui(InFrame, InDelta, Logical, Size, GuiData);
	UpdateShadowLight(InFrame);
	HandleProfilingActions(Actions);
	const bool bCaptureRdc = HandleCaptureActions(
	    Actions, std::binary_search(Options.RdcFrames.begin(), Options.RdcFrames.end(), InFrame + 1));
	if (Actions.bSave)
	{
		SaveSettingsAsync(Options.Config);
		Log(ELogLevel::Info, "Experiment save queued: " + Options.Config.string());
	}
	if (ModelPlugin)
	{
		ModelPlugin->Input(Window->Events(), Gui && Settings.bShowGui && Gui->WantsMouse(),
		                   Gui && Settings.bShowGui && Gui->WantsKeyboard());
	}
	const bool bTakeCapture = Actions.bCapture || (!Options.Capture.empty() && InFrame == Options.Frames - 1);
	if (ScenePlugin)
	{
		ScenePlugin->Input(Options.bBenchmarkCamera ? std::span<const FInputEvent>{} : Window->Events(),
		                   Gui && Settings.bShowGui && Gui->WantsMouse(),
		                   Gui && Settings.bShowGui && Gui->WantsKeyboard());
	}
	RenderFrame(InFrame, Size, std::move(GuiData), bTakeCapture, bCaptureRdc);
	ProfileFrame();
}

FRenderFrame FViewerApplication::UpdateScene(FSize InSize)
{
	HYP_PERF_SCOPE_C(Frame, UpdateScene);
	FRenderFrame Frame{InSize, Settings};
	Frame.View.Width = std::max(1u, InSize.Width);
	Frame.View.Height = std::max(1u, InSize.Height);
	for (const auto& Plugin : Plugins->GetInstances())
	{
		if (auto Scene = dynamic_cast<IScenePlugin*>(Plugin.get()))
		{
			Scene->Update(Frame);
		}
	}
	Frame.View.bInstanceBatching &= !Options.bNoInstanceBatching;
	return Frame;
}

FRenderGraph FViewerApplication::BuildRenderGraph(const FRenderFrame& InFrame, const FGuiDrawData& InGuiData,
                                                  std::shared_ptr<const FMaterialFrameContext> InMaterialFrame,
                                                  const FCascadedShadowSettings& InShadows)
{
	FRenderGraph Graph;
	ForwardPipeline->Build(
	    Graph, InFrame.View, std::move(InMaterialFrame), InShadows,
	    {float(InFrame.Settings.ClearRed), float(InFrame.Settings.ClearGreen), float(InFrame.Settings.ClearBlue), 1},
	    [&](FRenderGraph& InGraph)
	    {
		    for (const auto& Plugin : Plugins->GetInstances())
		    {
			    if (Plugin.get() == GuiPlugin)
			    {
				    GuiPlugin->BuildDeferred(InGraph, InGuiData);
			    }
			    else if (auto Render = dynamic_cast<IRenderPlugin*>(Plugin.get()))
			    {
				    Render->Build(InGraph, InFrame);
			    }
		    }
	    },
	    true);
	return Graph;
}

} // namespace Hyperion
