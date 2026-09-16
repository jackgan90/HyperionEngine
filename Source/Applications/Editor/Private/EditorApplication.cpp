#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include <algorithm>
#include <fstream>
#include <thread>

namespace Hyperion
{
FEditorApplication::FEditorApplication(FEditorOptions InOptions)
    : Options(std::move(InOptions)), Files(LoadContentMounts(Options.Mounts)), IO(Tasks, Files), Assets(IO)
{
	RegisterSceneAssetTypes(Assets.Types());
}

FEditorApplication::~FEditorApplication()
{
	try
	{
		Shutdown();
	}
	catch (const std::exception& Failure)
	{
		Log(ELogLevel::Error, Failure.what());
	}
}

void FEditorApplication::LoadCatalogs()
{
	for (const auto& Mount : Files->GetMounts())
	{
		try
		{
			const auto Catalog = Assets.LoadAsync<FAssetCatalog>(Mount.Root / "Catalog.hasset").Get(Tasks);
			Assets.AddCatalog(*Catalog, Mount.Root);
			for (const auto& Reference : Catalog->Assets)
			{
				if (Reference.TypeId == "hyperion.scene")
				{
					const auto Path = std::filesystem::path(Reference.Path);
					ScenePaths.push_back((Path.is_absolute() ? Path : Mount.Root / Path).generic_string());
				}
			}
		}
		catch (const std::exception& Failure)
		{
			CatalogError += Mount.Root.generic_string() + ": " + Failure.what() + "\n";
		}
	}
	std::sort(ScenePaths.begin(), ScenePaths.end());
	ScenePaths.erase(std::unique(ScenePaths.begin(), ScenePaths.end()), ScenePaths.end());
}

void FEditorApplication::Initialize()
{
	LoadCatalogs();
	Window = std::make_unique<FWindow>("Hyperion Editor", FSize{1600, 960}, Options.bHidden);
	if (!Window->SetDarkTitleBar(true))
	{
		Log(ELogLevel::Warning, "Native dark title bar is unavailable on this platform");
	}
	const auto Surface = Window->Surface();
	const auto Size = Window->PixelSize();
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          FRHIBackendRegistry Backends;
		                          RegisterD3D12RHIBackend(Backends);
		                          Device = Backends.CreateDevice(ERHIBackend::D3D12, {});
		                          Swapchain = Device->CreateSwapchain({Surface, Size, ERHIDepthFormat::D32, 0});
	                          }));
	Compiler = std::make_unique<FShaderCompiler>("/Engine/Shaders",
	                                             std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache", Files);
	Session = std::make_unique<FRenderSession>(Tasks, *Device, *Compiler);
	Pipeline = std::make_unique<FSceneRenderPipeline>(*Session, Device->GetCapabilities());
	Gui = std::make_unique<FGui>(Window.get());
	Gui->UseEditorStyle();
	const auto FontBytes = IO.ReadAsync("/Engine/Fonts/RobotoMedium.ttf").Get(Tasks);
	Gui->LoadFont(*FontBytes, 15);
	if (!Options.bExercise)
	{
		std::ifstream Stream(Options.Layout, std::ios::binary);
		if (Stream)
		{
			const std::string Layout{std::istreambuf_iterator<char>(Stream), {}};
			Gui->LoadLayout(Layout);
		}
	}
	GuiRenderer = std::make_unique<FGuiRenderer>(*Device, *Compiler, Tasks, Gui->FontImage(), &Session->GetResources());
	GuiRenderer->Start();
	Scene = std::make_unique<FSceneInstance>(*Session, Tasks, Assets);
	if (!Options.Scene.empty())
	{
		OpenScene(Options.Scene);
	}
}

void FEditorApplication::OpenScene(const std::string& InPath)
{
	Camera.Reset();
	bCameraDragging = false;
	Selection.reset();
	Error.clear();
	bReadyLogged = false;
	ReadyFrames = 0;
	try
	{
		Scene->Load(InPath);
		CurrentPath = InPath;
		++OpenCount;
		Log(ELogLevel::Info, "Editor opening scene: " + CurrentPath);
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

void FEditorApplication::SaveLayout()
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

void FEditorApplication::Shutdown()
{
	if (bStopped)
	{
		return;
	}
	Camera.Reset();
	Scene.reset();
	Assets.Drain();
	if (GuiRenderer)
	{
		GuiRenderer->Stop();
		GuiRenderer.reset();
	}
	Pipeline.reset();
	ViewportTarget = {};
	if (Session)
	{
		Session->Close();
		Session.reset();
	}
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          if (Device)
		                          {
			                          Device->WaitIdle();
			                          DeviceStats = Device->Statistics();
		                          }
		                          Swapchain.reset();
		                          Device.reset();
	                          }));
	Gui.reset();
	Compiler.reset();
	Window.reset();
	bStopped = true;
	Log(ELogLevel::Info, "Editor GPU frames: " + std::to_string(DeviceStats.SubmittedFrames) +
	                         "; validation errors: " + std::to_string(DeviceStats.ValidationErrors));
	if (DeviceStats.ValidationErrors)
	{
		throw std::runtime_error("Editor GPU validation errors");
	}
}

void FEditorApplication::Run()
{
	Initialize();
	const auto Started = ClockNanoseconds();
	auto Previous = Started;
	while (!Window->ShouldClose() && (Options.bExercise || !Options.Frames || FrameCount < Options.Frames))
	{
		Window->Poll();
		const auto Now = ClockNanoseconds();
		if (Options.bExercise && Now - Started > 75'000'000'000ull)
		{
			throw std::runtime_error("Editor interaction acceptance timed out");
		}
		const float Delta = static_cast<float>(double(Now - Previous) / 1e9);
		Previous = Now;
		if (Window->Minimized() || !Window->PixelSize().Width || !Window->PixelSize().Height)
		{
			Camera.Reset();
			bCameraDragging = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}
		Scene->Tick();
		bLoadErrorObserved |= !Scene->GetStatus().Error.empty();
		std::vector<FInputEvent> Events(Window->Events().begin(), Window->Events().end());
		if (Options.bExercise)
		{
			ExerciseInput(Events);
		}
		auto Data = DrawGui(std::clamp(Delta, .001f, .1f), Events);
		RouteCamera(Options.bExercise ? 1.f / 60 : Delta, Events);
		Scene->Tick();
		// Async scene readiness is independent of render frame rate.
		const bool bExerciseComplete = Options.bExercise && ExerciseStep == 21 && ReadyFrames > 8;
		const bool bCapture =
		    !Options.Capture.empty() &&
		    (bExerciseComplete || (!Options.bExercise && Options.Frames && FrameCount + 1 == Options.Frames));
		Render(std::move(Data), bCapture);
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
		if (bExerciseComplete)
		{
			break;
		}
	}
	SaveLayout();
	WriteReport();
	Shutdown();
	if (Options.bExercise && (!CurrentPath.ends_with("/Sponza.hasset") || OpenCount < 2 || !ReadyFrames ||
	                          !bMovementVerified || !bMovementGateVerified || !bRightReleaseVerified ||
	                          !bLookVerified || !bDollyVerified || !bSpeedVerified || !bInputIsolationVerified))
	{
		throw std::runtime_error("Editor interaction acceptance did not complete");
	}
}
} // namespace Hyperion
