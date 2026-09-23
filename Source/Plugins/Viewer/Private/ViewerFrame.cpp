#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Gui/GuiContributions.h"
#include "ViewerApplication.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace Hyperion
{
void FViewerPlugin::ValidateRun()
{
	if (Options.bBenchmarkCamera && !ScenePlugin && !ModelPlugin)
	{
		throw std::invalid_argument("--benchmark-camera requires scene-viewer or model-viewer");
	}
	if (Options.bExerciseContactShadows)
	{
		if (!Gui || !Settings.bShowGui || !ScenePlugin || Settings.RenderPipeline != "deferred")
		{
			throw std::invalid_argument(
			    "Contact UI exercise requires scene-viewer, Deferred and a visible debug panel");
		}
		Settings.bContactShadows = false;
		Settings.ContactShadowDebug = 0;
	}
	if (Options.bExerciseRdcUi && (!Gui || !Settings.bShowGui))
	{
		throw std::runtime_error("RDC UI exercise requires a visible diagnostics panel");
	}
}

void FViewerPlugin::Update(const FPluginUpdate& InUpdate)
{
	if (bFinished)
	{
		return;
	}
	const auto Frame = static_cast<int>(InUpdate.Frame);
	if (Window->ShouldClose() || (Options.Frames && Frame >= Options.Frames))
	{
		Finish();
		return;
	}
	UpdateProfiling(Frame);
	HYP_PERF_SCOPE_NAMED(EProfileCategory::Frame, "ApplicationFrame", FrameScope);
	HYP_PERF_VALUE(FrameScope, Frame);
	FrameStartedAt = ClockNanoseconds();
	DeltaSeconds = Frame ? InUpdate.DeltaSeconds : 1.f / 60.f;
	ExerciseBenchmarkCamera(Frame);
	Tick(Frame, DeltaSeconds);
	if (!PendingFrames.empty() && PendingFrames.back().Frame == Frame)
	{
		PendingFrames.back().MainMilliseconds = double(ClockNanoseconds() - FrameStartedAt) / 1e6;
	}
	CollectFrames();
}

void FViewerPlugin::PollInput()
{
	HYP_PERF_SCOPE_C(Frame, PollInput);
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

void FViewerPlugin::ExerciseWindow(int InFrame)
{
	if (!Options.bExercise)
	{
		return;
	}
	if (Options.bExerciseContactShadows)
	{
		if (!bContactExerciseCompleted)
		{
			return;
		}
		InFrame = static_cast<int>(ContactWindowFrame++);
		if (InFrame >= 36 && !bContactWindowCompleted)
		{
			std::uint64_t ExpectedBytes{};
			for (std::uint32_t Mip = 0; (960U >> Mip) || (540U >> Mip); ++Mip)
			{
				ExpectedBytes += std::uint64_t(std::max(1U, 960U >> Mip)) * std::max(1U, 540U >> Mip) * 4;
			}
			const auto Pixels = Window->PixelSize();
			bContactWindowCompleted = Pixels.Width == 960 && Pixels.Height == 540 &&
			                          PipelineStatistics.bContactShadows &&
			                          PipelineStatistics.HierarchicalDepth.Bytes == ExpectedBytes;
			if (bContactWindowCompleted)
			{
				Log(ELogLevel::Info, "Contact GUI verified: active resize/minimize/restore at 960x540");
			}
		}
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

FDebugActions FViewerPlugin::BuildGui(int InFrame, float InDelta, FSize InLogical, FSize InPixels,
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
		ExerciseContactInput(UiEvents);
		Gui->BeginFrame(InLogical, InPixels, InDelta, UiEvents);
		Context->Publish(FDebugPanelEvent{*Gui, Settings, Metrics, InLogical, Actions});
		RdcButtonBounds = Actions.CaptureRdcBounds;
		ContactShadowBounds = Actions.ContactShadowBounds;
		if (ScenePlugin)
		{
			ScenePlugin->DrawGui(*Gui, SceneStatistics, Options.bNoInstanceBatching, PipelineStatistics.LocalLights);
		}
		if (ScenePlugin || ModelPlugin)
		{
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
		Context->Publish(FGuiPanelEvent{*Gui});
		OutData = Gui->Render();
	}
	return Actions;
}

void FViewerPlugin::Tick(int InFrame, float InDelta)
{
	PollInput();
	if (Options.bExerciseDepthConfig && InFrame == 2)
	{
		Settings.bReversedZ = !bActiveReversedZ;
	}
	ExerciseWindow(InFrame);
	const auto Size = Window->PixelSize();
	const auto Logical = Window->LogicalSize();
	if (Gui && (!Settings.bShowGui || Window->Minimized() || !Size.Width || !Size.Height))
	{
		// Discard inactive widget buffers before agent edits can replace their authored values.
		Gui->FinishEditing();
		Gui->ResetInput();
	}
	if (Window->Minimized() || !Size.Width || !Size.Height)
	{
		if (ScenePlugin)
		{
			ScenePlugin->Input(Window->Events(), true, true);
		}
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

FRenderFrame FViewerPlugin::UpdateScene(FSize InSize)
{
	HYP_PERF_SCOPE_C(Frame, UpdateScene);
	FRenderFrame Frame{InSize, Settings};
	// Editable settings describe the next launch; every submitted frame uses the startup mode.
	Frame.Settings.bReversedZ = bActiveReversedZ;
	Frame.View.DepthConvention = GetDepthConvention(bActiveReversedZ);
	Frame.View.Width = std::max(1u, InSize.Width);
	Frame.View.Height = std::max(1u, InSize.Height);
	Frame.DeltaSeconds = DeltaSeconds;
	if (SceneProducer)
	{
		SceneProducer->Update(Frame);
	}
	Frame.View.bInstanceBatching &= !Options.bNoInstanceBatching;
	if (Frame.SceneView)
	{
		Frame.SceneView->bInstanceBatching &= !Options.bNoInstanceBatching;
	}
	return Frame;
}

FRenderGraph FViewerPlugin::BuildRenderGraph(const FRenderFrame& InFrame, const FGuiDrawData& InGuiData,
                                             std::shared_ptr<const FMaterialFrameContext> InMaterialFrame,
                                             std::shared_ptr<const FSceneFrameSeed> InSceneSeed,
                                             const FCascadedShadowSettings& InShadows)
{
	FRenderGraph Graph;
	FScenePipelineSettings Pipeline;
	Pipeline.Pipeline =
	    InFrame.Settings.RenderPipeline == "deferred" ? ESceneRenderPipeline::Deferred : ESceneRenderPipeline::Forward;
	Pipeline.GBuffer = InFrame.Settings.GBufferLayout == "high" ? FGBufferLayout::HighPrecision() : FGBufferLayout{};
	Pipeline.Exposure = float(InFrame.Settings.Exposure);
	Pipeline.DebugMode = static_cast<std::uint32_t>(InFrame.Settings.GBufferDebug);
	Pipeline.bClusteredLighting = InFrame.Settings.bClusteredLighting;
	Pipeline.ContactShadows = {InFrame.Settings.bContactShadows,
	                           float(InFrame.Settings.ContactShadowLength),
	                           float(InFrame.Settings.ContactShadowThickness),
	                           float(InFrame.Settings.ContactShadowBias),
	                           static_cast<std::uint32_t>(InFrame.Settings.ContactShadowSteps),
	                           static_cast<std::uint32_t>(InFrame.Settings.ContactShadowDebug),
	                           static_cast<std::uint32_t>(InFrame.Settings.HierarchicalDepthMip)};
	ScenePipeline->Configure(Pipeline);
	const auto Extensions = [&](FRenderGraph& InGraph)
	{
		if (GuiRenderer)
		{
			GuiRenderer->BuildDeferred(InGraph, InGuiData);
		}
	};
	const FVec4 Clear{float(InFrame.Settings.ClearRed), float(InFrame.Settings.ClearGreen),
	                  float(InFrame.Settings.ClearBlue), 1};
	if (InFrame.SceneView)
	{
		ScenePipeline->Build(Graph, *InFrame.SceneView, std::move(InSceneSeed), InShadows, Clear, Extensions, true);
	}
	else
	{
		ScenePipeline->Build(Graph, InFrame.View, std::move(InMaterialFrame), InShadows, Clear, Extensions, true);
	}

	return Graph;
}

} // namespace Hyperion
