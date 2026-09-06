#include "Hyperion/Core/Core.h"
#include "ViewerApplication.h"
#include "ViewerCaptureScope.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace Hyperion
{
void FViewerApplication::RunFrames()
{
	auto LastFrame = ClockNanoseconds();
	if (Options.bExerciseRdcUi && (!Gui || !Settings.bShowGui))
	{
		throw std::runtime_error("RDC UI exercise requires a visible diagnostics panel");
	}
	for (int Frame = 0; !Window->ShouldClose() && (!Options.Frames || Frame < Options.Frames); ++Frame)
	{
		FProfileScope Scope("Application frame");
		const auto Now = ClockNanoseconds();
		const float Delta = Frame ? float(Now - LastFrame) / 1e9f : 1.f / 60.f;
		LastFrame = Now;
		Tick(Frame, Delta);
	}
}

void FViewerApplication::PollInput()
{
	Window->Poll();
	Services->Tasks.PumpMain();
	if (ModelPlugin)
	{
		Metrics.AssetStatus = ModelPlugin->Status();
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
	UpdateCaptureStatus();
	FDebugActions Actions;
	if (Gui && Settings.bShowGui)
	{
		std::vector<FInputEvent> UiEvents(Window->Events().begin(), Window->Events().end());
		ExerciseCaptureInput(std::binary_search(Options.RdcFrames.begin(), Options.RdcFrames.end(), InFrame + 1),
		                     UiEvents);
		Gui->BeginFrame(InLogical, InPixels, InDelta, UiEvents);
		Actions = DrawDebugPanel(*Gui, Settings, Metrics, InLogical);
		RdcButtonBounds = Actions.CaptureRdcBounds;
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
	HandleCaptureActions(Actions, std::binary_search(Options.RdcFrames.begin(), Options.RdcFrames.end(), InFrame + 1));
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
	auto Screenshot = RenderFrame(Size, GuiData, bTakeCapture);
	if (bTakeCapture)
	{
		SaveScreenshot(std::move(Screenshot));
	}
	ProfileFrame();
}

FImage FViewerApplication::RenderFrame(FSize InSize, const FGuiDrawData& InGuiData, bool bInTakeCapture)
{
	auto& Tasks = Services->Tasks;
	FImage Screenshot;
#if HYP_ENABLE_RENDERDOC
	const auto Surface = Window->Surface();
	bool bRdcSucceeded = false;
#endif
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          FProfileScope Prepare("Render preparation");
#if HYP_ENABLE_RENDERDOC
		                          FFrameCaptureScope CaptureScope(Tasks, FrameCapture, Surface);
#endif
		                          if (GuiPlugin)
		                          {
			                          Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                                                    [&]
			                                                    {
				                                                    GuiPlugin->Prepare(InGuiData);
			                                                    }));
		                          }
		                          FRenderGraph Graph;
		                          FColorPass Clear;
		                          Clear.Commands.Name = "Clear";
		                          Clear.Load = EColorLoad::Clear;
		                          Clear.Commands.ClearColor = {float(Settings.ClearRed), float(Settings.ClearGreen),
		                                                       float(Settings.ClearBlue), 1};
		                          Graph.Add(std::move(Clear));
		                          for (const auto& Plugin : Plugins->GetInstances())
		                          {
			                          if (auto Render = dynamic_cast<IRenderPlugin*>(Plugin.get()))
			                          {
				                          Render->Build(Graph, {InSize, Settings});
			                          }
		                          }
		                          Screenshot =
		                              ExecuteGraph(Graph, Tasks, *Swapchain, InSize, Settings.bVsync, bInTakeCapture);
#if HYP_ENABLE_RENDERDOC
		                          bRdcSucceeded = CaptureScope.Finish();
#endif
		                          Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                                                    [&]
		                                                    {
			                                                    Metrics.Device = Device->Statistics();
		                                                    }));
	                          }));
#if HYP_ENABLE_RENDERDOC
	if (bRdcSucceeded && Settings.bRenderDocAutoOpen)
	{
		FrameCapture->OpenLastCapture();
	}
#endif
	if (Metrics.Device.ValidationErrors)
	{
		throw std::runtime_error("RHI validation errors");
	}
	return Screenshot;
}
} // namespace Hyperion
