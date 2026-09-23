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
	Gui->SetPathDisplayRoot("/Game");
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
	InitializeSceneDocument();
	AssetWorkspace = std::make_unique<FAssetWorkspace>(Assets, Tasks, *Session, Device->GetCapabilities());
	Error = Context.Require<FContentRootService>().StartupError;
	InitializePlacement();
	auto& Content = Context.Require<FContentRootService>();
	Context.Defer(
	    [this, &Content]
	    {
		    Content.UnregisterParticipant(*this);
	    });
	Content.RegisterParticipant(*this);
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
	try
	{
		LoadSceneDocument(InPath, false);
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

void FEditorPlugin::LoadSceneDocument(const std::string& InPath, bool bInDiscard)
{
	if (PendingSave)
	{
		throw FSceneEditError("busy", "Wait for the scene save to complete");
	}
	if (IsDirty() && !bInDiscard)
	{
		throw FSceneEditError("dirty_document",
		                      "Save the scene or explicitly discard changes before opening another scene");
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
	if (InPath.empty())
	{
		Scene->Close();
		InitializeSceneDocument();
	}
	else
	{
		Scene->Load(InPath);
	}
	CancelPlacement();
	PlacementModels.clear();
	PlacementPublication.reset();
	PlacementPublicationPreview.reset();
	ResetDocument();
	SceneDocument.SetPath(InPath);
	++OpenCount;
	Log(ELogLevel::Info, "Editor opening scene: " + CurrentPath);
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
	if (AssetWorkspace)
	{
		CloseAssetWindow();
	}
	AssetWorkspace.reset();
	SceneDocument.Detach(Tasks);
	SceneTarget.reset();
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

void FEditorPlugin::CollectEditorInput(std::vector<FInputEvent>& InEvents, std::vector<FInputEvent>& InAssetEvents)
{
	if (Options.bExercise)
	{
		ExerciseInput(InEvents);
	}
	if (!Options.ExerciseDocument.empty())
	{
		ExerciseDocumentInput(InEvents);
	}
	if (!Options.ExerciseViews.empty())
	{
		ExerciseViewInput(InEvents);
	}
	if (!Options.ExerciseCapture.empty())
	{
		ExerciseCaptureInput(InEvents);
	}
	if (!Options.ExerciseAssets.empty())
	{
		const bool bAssetInput = (ExerciseStep >= 13 && ExerciseStep <= 20) ||
		                         (ExerciseStep >= 100 && ExerciseStep <= 172) ||
		                         (ExerciseStep >= 190 && ExerciseStep != 200 && ExerciseStep != 201);
		ExerciseAssetInput(bAssetInput ? InAssetEvents : InEvents);
	}
	RouteHistoryShortcuts(InEvents);
	if (Options.bExerciseGizmo)
	{
		ExerciseGizmoInput(InEvents);
	}
	if (Options.bExercisePicking)
	{
		ExercisePickingInput(InEvents);
	}
	if (Options.bExerciseMultiSelection)
	{
		ExerciseMultiSelection(InEvents);
	}
	if (!Options.ExercisePlacement.empty())
	{
		ExercisePlacementInput(InEvents);
	}
	if (!Options.ExerciseOutlines.empty())
	{
		ExerciseOutlines();
	}
	if (!Options.ExerciseContent.empty())
	{
		ExerciseContentInput(InEvents);
	}
}

FGuiDrawData FEditorPlugin::DrawMainWindow(float InDelta, std::span<const FInputEvent> InEvents, bool bInDrawable)
{
	FGuiDrawData Data;
	{
		HYP_PERF_SCOPE_C(Frame, EditorGui);
		FMeasurementScope Measurement(!Options.Benchmark.empty(), BenchmarkFrame.GuiMilliseconds);
		if (bInDrawable)
		{
			Data = DrawGui(InDelta, InEvents);
		}
		else
		{
			bViewportVisible = false;
			Gui->ResetInput();
			ViewportRegion = {};
			CancelPlacement();
			ViewportClick.reset();
			FinishGizmo();
			Camera.SuspendInput(InEvents);
			bCameraDragging = false;
		}
	}
	if (!Options.ExercisePlacement.empty())
	{
		CheckPlacementMarkerDraws(Data);
	}
	if (Options.bExerciseMultiSelection)
	{
		CheckMultiSelectionMarkerDraws(Data);
	}
	return Data;
}

bool FEditorPlugin::AdvanceFrame(float InDelta)
{
	const bool bMainDrawable = !Window->Minimized() && Window->PixelSize().Width && Window->PixelSize().Height;
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
	std::vector<FInputEvent> AssetEvents;
	if (AssetWindow)
	{
		const auto NativeEvents = AssetWindow->NativeWindow().Events();
		AssetEvents.assign(NativeEvents.begin(), NativeEvents.end());
	}
	CollectEditorInput(Events, AssetEvents);
	// Synthetic clicks use a fixed GUI clock: hidden swapchains can run fast enough
	// to merge separate double-click sequences after the content grid scrolls.
	const float GuiDelta = Options.ExerciseAssets.empty() ? InDelta : 1.f / 60;
	auto Data = DrawMainWindow(GuiDelta, Events, bMainDrawable);
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
	const auto AssetCapture =
	    !Options.ExerciseAssets.empty() ? std::exchange(PlacementCapture, {}) : std::filesystem::path{};
	{
		HYP_PERF_SCOPE_C(Frame, EditorRenderWait);
		FMeasurementScope Measurement(!Options.Benchmark.empty(), BenchmarkFrame.RenderMilliseconds);
		if (bMainDrawable)
		{
			Render(std::move(Data), bCapture);
		}
	}
	AdvanceAssetWindow(GuiDelta, std::move(AssetEvents), AssetCapture);
	if (!bMainDrawable && (!AssetWindow || !AssetWindow->IsDrawable()))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
	Context.Provide(SceneDocument);
	Context.Provide<ISceneDocumentHost>(*this);
	Context.Provide<ISceneViewport>(*this);
	Context.Provide<IScenePlacement>(*this);
	Context.Provide<IRenderOutput>(*this);
	Context.Provide<IRenderCaptureControl>(*this);
	Context.Provide<IRenderDiagnostics>(*this);
	Context.Provide<IApplicationClose>(*this);
	Context.Provide<IAssetWorkspace>(*AssetWorkspace);
	Context.Provide<IAssetPreviewWorkspace>(*AssetWorkspace);
}

void FEditorPlugin::Update(const FPluginUpdate& InUpdate)
{
	if (bFinished)
	{
		return;
	}
	PollSave();
	const auto SavedAssets = AssetWorkspace->Poll();
	if (!SavedAssets.empty())
	{
		SceneDocument.AssetsRefreshed();
		Scene->RefreshAssets(SavedAssets);
		RefreshContent();
	}
	PollContent();
	if (AssetWindow)
	{
		AssetWindow->Poll(IsAssetWindowBlocked());
		if (AssetWindow->ShouldClose() && !IsAssetWindowBlocked())
		{
			CloseAssetWindow();
		}
	}
	ProcessContentRoot();
	PollSavedClose();
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
	     !Options.ExerciseOutlines.empty() || !Options.ExerciseCapture.empty() || !Options.ExerciseContent.empty() ||
	     !Options.ExerciseAssets.empty()) &&
	    InUpdate.ElapsedSeconds > 90)
	{
		throw std::runtime_error("Editor interaction acceptance timed out at step " + std::to_string(ExerciseStep) +
		                         ": " + AssetWorkspace->ActiveStatus());
	}
	if (AdvanceFrame(InUpdate.DeltaSeconds))
	{
		Finish();
	}
	UpdateDocumentInteraction();
	AssetWorkspace->SetHostBlocked(IsAssetWindowBlocked() || bFinished);
}

void FEditorPlugin::Finish()
{
	if (!Options.ExerciseAssets.empty() && !bAssetsVerified)
	{
		throw std::runtime_error("Asset editor acceptance incomplete");
	}
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
