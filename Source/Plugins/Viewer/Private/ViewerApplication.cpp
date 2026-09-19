#include "ViewerApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include <algorithm>

namespace Hyperion
{
FViewerServices::FViewerServices(FPluginContext& InContext)
    : Tasks(InContext.Require<FTaskSystem>()), IO(InContext.Require<FIOService>()),
      Assets(InContext.Require<FAssetService>())
{
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

FViewerPlugin::FViewerPlugin(FOptions InOptions, FAppSettings InSettings)
    : Options(std::move(InOptions)), Settings(std::move(InSettings)), bActiveReversedZ(Settings.bReversedZ)
{
	Metrics.bActiveReversedZ = bActiveReversedZ;
	InitializeShadowSettings();
}

FViewerPlugin::~FViewerPlugin()
{
	Stop();
}

void FViewerPlugin::InitializePlugins()
{
	SceneProducer = Context->Find<IScenePlugin>();
	ScenePlugin = Context->Find<ISceneEditor>();
	ModelPlugin = SceneProducer && SceneProducer->HasScene() && !ScenePlugin ? SceneProducer : nullptr;
	Gui = Context->Find<FGui>();
	GuiRenderer = Context->Find<FGuiRenderer>();
	ScenePipeline = std::make_unique<FSceneRenderPipeline>(*RenderSession, Device->GetCapabilities(), Options.Pipeline,
	                                                       Context->Require<FRenderFeatureRegistry>().Create());
	if (ScenePlugin)
	{
		ScenePlugin->SetCullingMode(Options.SceneCulling == "none"     ? ESceneCullingMode::None
		                            : Options.SceneCulling == "linear" ? ESceneCullingMode::Linear
		                                                               : ESceneCullingMode::Bvh);
	}
}

FDeviceStats FViewerPlugin::ReleaseGraphics()
{
	FDeviceStats Stats;
	auto& Tasks = Services->Tasks;
	// Drain on normal exits and partial initialization before releasing any frame consumer.
	if (FramePipeline)
	{
		try
		{
			FramePipeline->Drain();
		}
		catch (const std::exception& Error)
		{
			Log(ELogLevel::Error, Error.what());
		}
		FramePipeline.reset();
		PendingFrames.clear();
	}
	ScenePipeline.reset();
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          if (Device)
		                          {
			                          Device->WaitIdle();
			                          BenchmarkGpu = Device->EndGpuTimingCapture();
		                          }
		                          if (Device)
		                          {
			                          Stats = Device->Statistics();
		                          }
	                          }));
	return Stats;
}

void FViewerPlugin::Shutdown()
{
	Services->Assets.Drain();
	Services->Tasks.WaitAll(Services->FileWrites);
	const auto Stats = ReleaseGraphics();
	for (const auto& Thread : Services->Tasks.Statistics())
	{
		Log(ELogLevel::Info, "Domain " + Thread.Name + ": " + std::to_string(Thread.Executed) + " tasks; thread ID " +
		                         std::to_string(Thread.ThreadId));
	}
	bStopped = true;
	Log(ELogLevel::Info, "GPU frames: " + std::to_string(Stats.SubmittedFrames) +
	                         "; validation errors: " + std::to_string(Stats.ValidationErrors));
	if (Stats.ValidationErrors)
	{
		throw std::runtime_error("RHI validation errors at shutdown");
	}
}

void FViewerPlugin::Start(FPluginContext& InContext)
{
	Context = &InContext;
	Control = &InContext.Require<FApplicationControl>();
	Services = std::make_unique<FViewerServices>(InContext);
	Window = &InContext.Require<FWindow>();
	Device = &InContext.Require<IRHIDevice>();
	Swapchain = &InContext.Require<IRHISwapchain>();
	Compiler = &InContext.Require<FShaderCompiler>();
	RenderSession = &InContext.Require<FRenderSession>();
	InitializeProfiling();
	InitializeCapture();
	InitializePlugins();
	Metrics.FrameLimits = {static_cast<std::uint32_t>(Settings.MainRenderLead),
	                       static_cast<std::uint32_t>(Settings.RenderRhiLead)};
	FramePipeline = std::make_unique<FFramePipeline>(Services->Tasks, Metrics.FrameLimits);
	ValidateRun();
	Log(ELogLevel::Info, "Windows rendering application started");
}

void FViewerPlugin::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	DrainFrames();
	VerifyOutputs();
	bVerified = true;
	Control->RequestExit();
}

void FViewerPlugin::Quiesce() noexcept
{
	SetProfilingMask(0);
	try
	{
		if (FramePipeline)
		{
			DrainFrames();
		}
	}
	catch (...)
	{
		Control->ReportFailure(std::current_exception());
	}
}

void FViewerPlugin::Stop() noexcept
{
	if (bStopped || !Services)
	{
		return;
	}
	Quiesce();
	try
	{
		Shutdown();
		if (bVerified)
		{
			SaveBenchmark();
		}
	}
	catch (...)
	{
		Control->ReportFailure(std::current_exception());
	}
	bStopped = true;
}
} // namespace Hyperion
