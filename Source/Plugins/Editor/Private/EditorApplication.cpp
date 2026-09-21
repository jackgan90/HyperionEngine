#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Core/Profiling/Measurement.h"
#include <algorithm>
#include <fstream>
#include <thread>

namespace Hyperion
{
FEditorPlugin::FEditorPlugin(FEditorOptions InOptions, FPluginContext& InContext)
    : Options(std::move(InOptions)), Files(&InContext.Require<FMountedFileSystem>()), Context(InContext),
      Control(InContext.Require<FApplicationControl>()), Tasks(InContext.Require<FTaskSystem>()),
      IO(InContext.Require<FIOService>()), Assets(InContext.Require<FAssetService>())
{
}

FEditorPlugin::~FEditorPlugin()
{
	Stop();
}

void FEditorPlugin::Stop() noexcept
{
	try
	{
		Shutdown();
	}
	catch (...)
	{
		Control.ReportFailure(std::current_exception());
	}
	bStopped = true;
}

void FEditorPlugin::InitializeContentBrowser()
{
	Browser = std::make_unique<FContentBrowser>(IO);
	RefreshContent();
}

void FEditorPlugin::Initialize()
{
	InitializeContentBrowser();
	Window = &Context.Require<FWindow>();
	Device = &Context.Require<IRHIDevice>();
	Swapchain = &Context.Require<IRHISwapchain>();
	Compiler = &Context.Require<FShaderCompiler>();
	Session = &Context.Require<FRenderSession>();
	Gui = &Context.Require<FGui>();
	GuiRenderer = &Context.Require<FGuiRenderer>();
	bInitialCapturePreference = Options.Preferences.bRenderDocCapture;
#if HYP_ENABLE_RENDERDOC
	FrameCapture = Context.Find<FFrameCapture>();
#endif
	auto Features = Context.Require<FRenderFeatureRegistry>().Create();
	Features.push_back(MakeTransientGeometryFeature());
	Features.push_back(MakeSelectionOutlineFeature(Device->GetCapabilities()));
	Pipeline = std::make_unique<FSceneRenderPipeline>(*Session, Device->GetCapabilities(), FScenePipelineSettings{},
	                                                  std::move(Features));
	Scene = std::make_unique<FSceneInstance>(*Session, Tasks, Assets, true);
	Error = Context.Require<FContentRootService>().StartupError;
	InitializePlacement();
	if (!Options.Scene.empty())
	{
		OpenScene(Options.Scene);
	}
}

void FEditorPlugin::OpenScene(const std::string& InPath)
{
	if (IsDirty() || PendingSave)
	{
		PendingOpen = InPath;
		bDiscardDialog = bRequestDiscard = true;
		return;
	}
	Camera.Reset();
	bViewportCameraInitialized = false;
	bCameraDragging = false;
	Selection.Clear();
	bSelectionInitialized = false;
	ViewportClick.reset();
	Error.clear();
	bReadyLogged = false;
	ReadyFrames = 0;
	try
	{
		Scene->Load(InPath);
		CancelPlacement();
		PlacementModels.clear();
		PlacementPublication.reset();
		PlacementPublicationPreview.reset();
		ResetDocument();
		CurrentPath = InPath;
		++OpenCount;
		Log(ELogLevel::Info, "Editor opening scene: " + CurrentPath);
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

void FEditorPlugin::SaveLayout()
{
	if (!Gui)
	{
		return;
	}
	const auto Layout = Gui->SaveLayout();
	if (!Options.Layout.parent_path().empty())
	{
		std::filesystem::create_directories(Options.Layout.parent_path());
	}
	std::ofstream Stream(Options.Layout, std::ios::binary);
	Stream << Layout;
	if (!Stream)
	{
		throw std::runtime_error("Could not save editor layout: " + Options.Layout.string());
	}
}

void FEditorPlugin::Shutdown()
{
	if (bStopped)
	{
		return;
	}
	CancelContentRequests();
	Camera.Reset();
	Scene.reset();
	Assets.Drain();
	PlacementModels.clear();
	PlacementIcons.clear();
	PlacementPublicationPreview.reset();
	PlacementMaterial.reset();
	PlacementLifetime.reset();
	Pipeline.reset();
	ViewportTarget = {};
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          if (Device)
		                          {
			                          Device->WaitIdle();
			                          DeviceStats = Device->Statistics();
		                          }
	                          }));
	bStopped = true;
	Log(ELogLevel::Info, "Editor GPU frames: " + std::to_string(DeviceStats.SubmittedFrames) +
	                         "; validation errors: " + std::to_string(DeviceStats.ValidationErrors));
	if (DeviceStats.ValidationErrors)
	{
		throw std::runtime_error("Editor GPU validation errors");
	}
}

bool FEditorPlugin::AdvanceFrame(float InDelta)
{
	BenchmarkFrame = {};
	const auto FrameStarted = Options.Benchmark.empty() ? 0 : ClockNanoseconds();
	{
		HYP_PERF_SCOPE_C(Frame, EditorSceneUpdate);
		FMeasurementScope Measurement(!Options.Benchmark.empty(), BenchmarkFrame.SceneMilliseconds);
		Scene->Tick();
	}
	PruneSelection();
	bLoadErrorObserved |= !Scene->GetStatus().Error.empty();
	InitializeViewportCamera();
	PollPlacementResources();
	std::vector<FInputEvent> Events(Window->Events().begin(), Window->Events().end());
	if (Options.bExercise)
	{
		ExerciseInput(Events);
	}
	if (!Options.ExerciseDocument.empty())
	{
		ExerciseDocumentInput(Events);
	}
	if (!Options.ExerciseViews.empty())
	{
		ExerciseViewInput(Events);
	}
	if (!Options.ExerciseCapture.empty())
	{
		ExerciseCaptureInput(Events);
	}
	RouteHistoryShortcuts(Events);
	if (Options.bExerciseGizmo)
	{
		ExerciseGizmoInput(Events);
	}
	if (Options.bExercisePicking)
	{
		ExercisePickingInput(Events);
	}
	if (Options.bExerciseMultiSelection)
	{
		ExerciseMultiSelection(Events);
	}
	if (!Options.ExercisePlacement.empty())
	{
		ExercisePlacementInput(Events);
	}
	if (!Options.ExerciseOutlines.empty())
	{
		ExerciseOutlines();
	}
	FGuiDrawData Data;
	if (!Options.ExerciseContent.empty())
	{
		ExerciseContentInput(Events);
	}
	{
		HYP_PERF_SCOPE_C(Frame, EditorGui);
		FMeasurementScope Measurement(!Options.Benchmark.empty(), BenchmarkFrame.GuiMilliseconds);
		Data = DrawGui(InDelta, Events);
	}
	if (!Options.ExercisePlacement.empty())
	{
		CheckPlacementMarkerDraws(Data);
	}
	if (Options.bExerciseMultiSelection)
	{
		CheckMultiSelectionMarkerDraws(Data);
	}
	{
		FMeasurementScope Measurement(!Options.Benchmark.empty(), BenchmarkFrame.SceneMilliseconds);
		Scene->Tick();
	}
	// Async scene readiness is independent of render frame rate.
	const bool bExerciseComplete = (Options.bExercise && ExerciseStep == 21 && ReadyFrames > 8) || bDocumentVerified ||
	                               bViewsVerified || bGizmoVerified || bPickingVerified || bPlacementVerified ||
	                               bOutlinesVerified || bMultiSelectionVerified || bContentVerified;
	const bool bCapture =
	    !Options.Capture.empty() &&
	    (bExerciseComplete || (!Options.bExercise && Options.Frames && FrameCount + 1 == Options.Frames) ||
	     (Options.ExerciseCapture == "toggle" && bPreferencesDialog && ExerciseStep == 2 && ExerciseWait == 2) ||
	     (Options.ExerciseCapture == "capture" && ExerciseStep == 1));
	{
		HYP_PERF_SCOPE_C(Frame, EditorRenderWait);
		FMeasurementScope Measurement(!Options.Benchmark.empty(), BenchmarkFrame.RenderMilliseconds);
		Render(std::move(Data), bCapture);
	}
	if (!Options.Benchmark.empty())
	{
		BenchmarkFrame.FrameMilliseconds = double(ClockNanoseconds() - FrameStarted) / 1e6;
		RecordBenchmark();
	}
	++FrameCount;
	if (Scene->GetStatus().bReady && !CurrentPath.empty())
	{
		++ReadyFrames;
		if (!bReadyLogged)
		{
			Log(ELogLevel::Info, "Editor scene ready: " + CurrentPath);
			bReadyLogged = true;
		}
	}
	return bExerciseComplete || (!Options.Benchmark.empty() && BenchmarkSamples.size() == Options.BenchmarkSamples);
}

void FEditorPlugin::Start(FPluginContext&)
{
	BenchmarkStarted = ClockNanoseconds();
	Initialize();
}

void FEditorPlugin::Update(const FPluginUpdate& InUpdate)
{
	if (bFinished)
	{
		return;
	}
	PollSave();
	PollContent();
	ProcessContentRoot();
	if (bFinished)
	{
		return;
	}
	if (PollClose() || (!Options.bExercise && Options.Frames && FrameCount >= Options.Frames))
	{
		Finish();
		return;
	}
	if ((Options.bExercise || Options.bExerciseGizmo || Options.bExercisePicking || Options.bExerciseMultiSelection ||
	     !Options.ExerciseDocument.empty() || !Options.ExerciseViews.empty() || !Options.ExercisePlacement.empty() ||
	     !Options.ExerciseOutlines.empty() || !Options.ExerciseCapture.empty() || !Options.ExerciseContent.empty()) &&
	    InUpdate.ElapsedSeconds > 90)
	{
		throw std::runtime_error("Editor interaction acceptance timed out");
	}
	if (Window->Minimized() || !Window->PixelSize().Width || !Window->PixelSize().Height)
	{
		CancelPlacement();
		ViewportClick.reset();
		FinishGizmo();
		Scene->Tick();
		Camera.Reset();
		bCameraDragging = false;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		return;
	}
	if (AdvanceFrame(InUpdate.DeltaSeconds))
	{
		Finish();
	}
}

void FEditorPlugin::Finish()
{
	if (!Options.ExerciseContent.empty() && (!bContentVerified || IsDirty() || !Window->ShouldClose()))
	{
		throw std::runtime_error("Content transition acceptance did not complete a clean close");
	}
	if (Options.bExerciseMultiSelection && !bMultiSelectionVerified)
	{
		throw std::runtime_error("Editor multi-selection acceptance did not complete");
	}
	if (!Options.ExerciseOutlines.empty() && !bOutlinesVerified)
	{
		throw std::runtime_error("Editor outline acceptance did not complete");
	}
	if (!Options.ExercisePlacement.empty() && !bPlacementVerified)
	{
		throw std::runtime_error("Editor placement acceptance did not complete");
	}
	if (Options.bExercisePicking && !bPickingVerified)
	{
		throw std::runtime_error("Editor picking acceptance did not complete");
	}
	if (Options.bExerciseGizmo && !bGizmoVerified)
	{
		throw std::runtime_error("Editor gizmo acceptance did not complete");
	}
	bFinished = true;
	SaveBenchmark();
	if (Options.Benchmark.empty())
	{
		SaveLayout();
	}
	WriteReport();
	Control.RequestExit();
	if (Options.bExercise && (!CurrentPath.ends_with("/Sponza.hasset") || OpenCount < 2 || !ReadyFrames ||
	                          !bMovementVerified || !bMovementGateVerified || !bRightReleaseVerified ||
	                          !bLookVerified || !bDollyVerified || !bSpeedVerified || !bInputIsolationVerified))
	{
		throw std::runtime_error("Editor interaction acceptance did not complete");
	}
}
} // namespace Hyperion
