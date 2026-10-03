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
	Rendering = Options.Rendering;
	Exposure = Rendering.Exposure;
	bInstanceBatching = Options.bInstanceBatching;
	CullingMode = Options.CullingMode;
	InitializeContentBrowser();
	Window = &Context.Require<FWindow>();
	Device = &Context.Require<IRHIDevice>();
	Swapchain = &Context.Require<IRHISwapchain>();
	Compiler = &Context.Require<FShaderCompiler>();
	Session = &Context.Require<FRenderSession>();
	Gui = &Context.Require<FGui>();
	Gui->SetPathDisplayRoot("/Game");
	Gui->SetAssetReferenceProvider(
	    [this](std::string_view InTypeId)
	    {
		    return AssetReferenceCandidates(InTypeId);
	    });
	GuiRenderer = &Context.Require<FGuiRenderer>();
	ImportPanel = std::make_unique<FAssetImportPanel>(Context.Find<FAssetImportWorkspace>(),
	                                                  Context.Require<FContentRootService>(), Options.Preferences,
	                                                  [this]()
	                                                  {
		                                                  SavePreferences();
	                                                  });
	Acceptance.Initialize(*this);
#if HYP_ENABLE_RENDERDOC
	FrameCapture = Context.Find<FFrameCapture>();
#endif
	SetRenderSettings(RenderSettingsRevision, Rendering);
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
		Transition.QueueOpen(InPath);
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
	Viewport.ResetNavigation();
	FrozenCullingView.reset();
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
	if (Gui)
	{
		Gui->SetAssetReferenceProvider({});
	}
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
	Viewport.ViewportTarget = {};
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
			Viewport.bViewportVisible = false;
			Gui->ResetInput();
			Viewport.ViewportRegion = {};
			CancelPlacement();
			CancelReparentGesture();
			ViewportClick.reset();
			FinishGizmo();
			Camera.SuspendInput(InEvents);
			Viewport.bCameraDragging = false;
		}
	}
	Acceptance.CheckGui(Data);
	return Data;
}

bool FEditorPlugin::AdvanceFrame(float InDelta)
{
	FrameIntervalMilliseconds = InDelta * 1000.0;
	const bool bMainDrawable = !Window->Minimized() && Window->PixelSize().Width && Window->PixelSize().Height;
	const bool bSceneReady = Scene->GetStatus().bReady;
	const auto ProfileFrameIndex = Options.Benchmark.empty() ? FrameCount : ReadyFrames;
	if (Options.Benchmark.empty() || bSceneReady)
	{
		UpdateProfilingSession(Options.Profiling, ProfileFrameIndex);
	}
	HYP_PERF_SCOPE_NAMED(EProfileCategory::Frame, "EditorApplicationFrame", EditorFrameScope);
	HYP_PERF_VALUE(EditorFrameScope, ProfileFrameIndex);
	BenchmarkFrame = {};
	const auto FrameStarted = Options.Benchmark.empty() ? 0 : ClockNanoseconds();
	BeginBenchmarkTiming();
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
	Acceptance.CollectInput(Events, AssetEvents);
	const float GuiDelta = Acceptance.Policy().GuiDelta.value_or(InDelta);
	auto Data = DrawMainWindow(GuiDelta, Events, bMainDrawable);
	{
		FMeasurementScope Measurement(!Options.Benchmark.empty(), BenchmarkFrame.SceneMilliseconds);
		Scene->Tick();
	}
	// Async scene readiness is independent of render frame rate.
	const bool bExerciseComplete = Acceptance.IsComplete();
	const bool bCapture =
	    !Options.Capture.empty() &&
	    (Acceptance.ShouldCapture() ||
	     (Acceptance.Policy().bUseFrameLimit && Options.Frames && FrameCount + 1 == Options.Frames) ||
	     (!Options.Benchmark.empty() && bBenchmarkTiming && BenchmarkSamples.size() + 1 == Options.BenchmarkSamples));
	const auto AssetCapture = Acceptance.TakeAssetCapture();
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
	FinishFrameTiming(FrameStarted, bSceneReady);
	return bExerciseComplete || (!Options.Benchmark.empty() && BenchmarkSamples.size() == Options.BenchmarkSamples);
}

void FEditorPlugin::Start(FPluginContext&)
{
	if (Options.LogHistory)
	{
		Context.Provide(*Options.LogHistory);
	}
	BenchmarkStarted = ClockNanoseconds();
	Initialize();
	Context.Provide(SceneDocument);
	Context.Provide<ISceneDocumentHost>(*this);
	Context.Provide<ISceneViewport>(*this);
	Context.Provide<IScenePlacement>(*this);
	Context.Provide<IRenderOutput>(*this);
	Context.Provide<IRenderCaptureControl>(*this);
	Context.Provide<IRenderDiagnostics>(*this);
	Context.Provide<IRenderSettings>(*this);
	Context.Provide<IShadowControls>(*this);
	Context.Provide<ISceneLightControls>(*this);
	Context.Provide<IProfilingControl>(*this);
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
		CancelPlacement();
		PlacementModels.clear();
		PlacementPublication.reset();
		PlacementPublicationPreview.reset();
		SceneDocument.AssetsRefreshed();
		Scene->RefreshAssets(SavedAssets);
		RefreshContent();
	}
	PollContent();
	if (auto* Imports = Context.Find<FAssetImportWorkspace>(); Imports && Imports->Revision() != ImportRevision)
	{
		ImportRevision = Imports->Revision();
		RefreshContent();
	}
	ImportPanel->Process(Window->Surface());
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
	if (PollClose() || (Acceptance.Policy().bUseFrameLimit && Options.Frames && FrameCount >= Options.Frames))
	{
		Finish();
		return;
	}
	Acceptance.CheckTimeout(InUpdate.ElapsedSeconds);
	if (AdvanceFrame(InUpdate.DeltaSeconds))
	{
		Finish();
	}
	UpdateDocumentInteraction();
	AssetWorkspace->SetHostBlocked(IsAssetWindowBlocked() || bFinished);
}

void FEditorPlugin::Finish()
{
	Acceptance.CheckCompletion();
	bFinished = true;
	SaveBenchmark();
	if (Options.Benchmark.empty())
	{
		SaveLayout();
	}
	WriteReport();
	Control.RequestExit();
}
} // namespace Hyperion
