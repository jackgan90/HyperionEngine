#include "ViewerApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Triangle/TrianglePlugin.h"
#include <algorithm>

namespace Hyperion
{
FViewerServices::FViewerServices(const FAppSettings& InSettings)
    : Tasks(static_cast<unsigned>(InSettings.Workers), static_cast<unsigned>(InSettings.RhiThreads)), IO(Tasks),
      Assets(IO)
{
	RegisterGltfImporter(Assets);
	RegisterSceneManifestLoader(Assets);
}

FViewerServices::~FViewerServices()
{
	DrainWrites();
}

void FViewerServices::DrainWrites() noexcept
{
	for (const auto& Write : FileWrites)
	{
		try
		{
			Tasks.Wait(Write);
		}
		catch (...)
		{
			// Normal shutdown observes errors; unwinding still joins every producer.
		}
	}
}

FViewerApplication::FViewerApplication(FOptions InOptions)
    : Options(std::move(InOptions)), Settings(LoadSettings(Options.Config))
{
	ApplyOptions(Options, Settings);
}

FViewerApplication::~FViewerApplication()
{
	SetProfilingMask(0);
	if (Services && !bStopped)
	{
		Services->DrainWrites();
		Services->Assets.Drain();
		try
		{
			ReleaseGraphics();
		}
		catch (const std::exception& Error)
		{
			Log(ELogLevel::Error, Error.what());
		}
	}
}

void FViewerApplication::InitializeGraphics()
{
	const auto SelectedBackend = ParseRHIBackend(Settings.RHIBackend);
	FRHIBackendRegistry Backends;
	RegisterD3D12RHIBackend(Backends);
	if (!Backends.IsRegistered(SelectedBackend))
	{
		throw std::runtime_error("RHI backend is not registered: " + Settings.RHIBackend);
	}
	Services = std::make_unique<FViewerServices>(Settings);
	Window = std::make_unique<FWindow>(
	    Settings.Title, FSize{static_cast<unsigned>(Settings.Width), static_cast<unsigned>(Settings.Height)},
	    Options.bHidden);
	const auto Surface = Window->Surface();
	const auto InitialSize = Window->PixelSize();
	auto& Tasks = Services->Tasks;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          FRHIDeviceDesc Desc;
		                          Desc.RequiredFeatures = {ERHIFeature::Graphics, ERHIFeature::TextureSampling};
		                          Device = Backends.CreateDevice(SelectedBackend, Desc);
		                          Swapchain = Device->CreateSwapchain({Surface, InitialSize});
	                          }));
	Compiler = std::make_unique<FShaderCompiler>(std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
	                                             std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache");
}

void FViewerApplication::InitializePlugins()
{
	auto Requested = Options.bVerifyClear ? std::vector<std::string>{} : Settings.Plugins;
	if (Options.bNoUi)
	{
		std::erase(Requested, std::string("debug-ui"));
	}
	std::erase(Requested, std::string("renderdoc"));
	FImage Font;
	if (std::find(Requested.begin(), Requested.end(), "debug-ui") != Requested.end())
	{
		Gui = std::make_unique<FGui>(Window.get());
		Font = Gui->FontImage();
	}
	auto& Tasks = Services->Tasks;
	RenderSession = std::make_unique<FRenderSession>(Tasks, *Device, *Compiler);
	FPluginRegistry Registry;
	RegisterTrianglePlugin(Registry, *RenderSession, *Device, *Compiler, Tasks);
	RegisterModelViewerPlugin(Registry, *RenderSession, Tasks, Services->Assets, Settings.ModelSource);
	RegisterSceneViewerPlugin(Registry, *RenderSession, Tasks, Services->Assets, Settings.SceneSource);
	RegisterDebugUiPlugin(Registry, *Device, *Compiler, Tasks, Font);
	Plugins = std::make_unique<FPluginSet>(Registry.Activate(Requested));
	for (const auto& Plugin : Plugins->GetInstances())
	{
		if (auto Debug = dynamic_cast<FDebugUiPlugin*>(Plugin.get()))
		{
			GuiPlugin = Debug;
		}
		if (auto Model = dynamic_cast<FModelViewerPlugin*>(Plugin.get()))
		{
			ModelPlugin = Model;
		}
		if (auto Scene = dynamic_cast<FSceneViewerPlugin*>(Plugin.get()))
		{
			ScenePlugin = Scene;
			ScenePlugin->SetCullingMode(Options.SceneCulling == "none"     ? ESceneCullingMode::None
			                            : Options.SceneCulling == "linear" ? ESceneCullingMode::Linear
			                                                               : ESceneCullingMode::Bvh);
		}
	}
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Metrics.Device = Device->Statistics();
	                          }));
}

FDeviceStats FViewerApplication::ReleaseGraphics()
{
	FDeviceStats Stats;
	auto& Tasks = Services->Tasks;
	Plugins.reset();
	GuiPlugin = nullptr;
	ModelPlugin = nullptr;
	ScenePlugin = nullptr;
	if (RenderSession)
	{
		RenderSession->Close();
		RenderSession.reset();
	}
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          if (Device)
		                          {
			                          Device->WaitIdle();
		                          }
		                          Swapchain.reset();
		                          if (Device)
		                          {
			                          Stats = Device->Statistics();
			                          Device.reset();
		                          }
#if HYP_ENABLE_RENDERDOC
		                          if (StartupPlugins)
		                          {
			                          StartupPlugins->Stop();
		                          }
#endif
	                          }));
	return Stats;
}

void FViewerApplication::Shutdown()
{
	Services->Assets.Drain();
	Services->Tasks.WaitAll(Services->FileWrites);
	const auto Stats = ReleaseGraphics();
	Gui.reset();
	for (const auto& Thread : Services->Tasks.Statistics())
	{
		Log(ELogLevel::Info, "Domain " + Thread.Name + ": " + std::to_string(Thread.Executed) + " tasks; thread ID " +
		                         std::to_string(Thread.ThreadId));
	}
	Services->Tasks.Shutdown();
	bStopped = true;
	Log(ELogLevel::Info, "GPU frames: " + std::to_string(Stats.SubmittedFrames) +
	                         "; validation errors: " + std::to_string(Stats.ValidationErrors));
	if (Stats.ValidationErrors)
	{
		throw std::runtime_error("RHI validation errors at shutdown");
	}
	if (MemoryStats(EMemoryTag::Gui).LiveBytes || MemoryStats(EMemoryTag::Render).LiveBytes)
	{
		throw std::runtime_error("GUI or renderer hooked allocations survived shutdown");
	}
}

void FViewerApplication::Run()
{
	InitializeProfiling();
	InitializeCapture();
	InitializeGraphics();
	InitializePlugins();
	Log(ELogLevel::Info, "Windows rendering application started");
	RunFrames();
	VerifyOutputs();
	Shutdown();
	Log(ELogLevel::Info, "Rendering lifecycle completed successfully");
}
} // namespace Hyperion
