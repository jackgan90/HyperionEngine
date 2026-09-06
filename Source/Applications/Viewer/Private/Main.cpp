#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "Hyperion/Triangle/TrianglePlugin.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/Capture/FrameCapture.h"
#include "Hyperion/RenderDoc/RenderDocPlugin.h"
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace
{
struct FOptions
{
	std::filesystem::path Config = std::filesystem::path(HYP_SOURCE_DIR) / "experiments/Triangle.json";
	std::filesystem::path Capture;
	std::filesystem::path SaveConfig;
	std::filesystem::path Model;
	bool VerifyModel{};
	std::optional<std::string> Backend;
	int Frames{};
	bool Hidden{};
	bool Exercise{};
	bool VerifyClear{};
	bool VerifyTriangle{};
	bool VerifyUi{};
	bool NoUi{};
	bool RenderDoc{};
	bool OpenRdc{};
	bool ExerciseRdcUi{};
	std::optional<std::string> RenderDocLibrary;
	std::optional<std::string> RdcOutput;
	std::vector<int> RdcFrames;
};

FOptions Parse(int InArgc, char** InArgv)
{
	FOptions O;
	for (int I = 1; I < InArgc; ++I)
	{
		const std::string Arg = InArgv[I];
		if (Arg == "--frames" && I + 1 < InArgc)
		{
			O.Frames = std::stoi(InArgv[++I]);
		}
		else if (Arg == "--config" && I + 1 < InArgc)
		{
			O.Config = InArgv[++I];
		}
		else if (Arg == "--capture" && I + 1 < InArgc)
		{
			O.Capture = InArgv[++I];
		}
		else if (Arg == "--model" && I + 1 < InArgc)
		{
			O.Model = InArgv[++I];
		}
		else if (Arg == "--verify-model")
		{
			O.VerifyModel = true;
		}
		else if (Arg == "--save-config" && I + 1 < InArgc)
		{
			O.SaveConfig = InArgv[++I];
		}
		else if (Arg == "--backend" && I + 1 < InArgc)
		{
			O.Backend = InArgv[++I];
		}
		else if (Arg == "--hidden")
		{
			O.Hidden = true;
		}
		else if (Arg == "--exercise-window")
		{
			O.Exercise = true;
		}
		else if (Arg == "--verify-clear")
		{
			O.VerifyClear = true;
		}
		else if (Arg == "--verify-triangle")
		{
			O.VerifyTriangle = true;
		}
		else if (Arg == "--verify-ui")
		{
			O.VerifyUi = true;
		}
		else if (Arg == "--no-ui")
		{
			O.NoUi = true;
		}
		else if (Arg == "--renderdoc")
		{
			O.RenderDoc = true;
		}
		else if (Arg == "--renderdoc-library" && I + 1 < InArgc)
		{
			O.RenderDocLibrary = InArgv[++I];
			O.RenderDoc = true;
		}
		else if (Arg == "--rdc-output" && I + 1 < InArgc)
		{
			O.RdcOutput = InArgv[++I];
		}
		else if (Arg == "--capture-rdc" && I + 1 < InArgc)
		{
			O.RdcFrames.push_back(std::stoi(InArgv[++I]));
			O.RenderDoc = true;
		}
		else if (Arg == "--open-rdc")
		{
			O.OpenRdc = true;
			O.RenderDoc = true;
		}
		else if (Arg == "--exercise-rdc-ui")
		{
			O.ExerciseRdcUi = true;
		}
		else
		{
			throw std::invalid_argument("Unknown or incomplete option: " + Arg);
		}
	}
	if (O.Frames < 0)
	{
		throw std::invalid_argument("--frames must be nonnegative");
	}
	if ((!O.Capture.empty() || O.VerifyClear || O.VerifyTriangle || O.VerifyUi || O.VerifyModel) && O.Frames == 0)
	{
		throw std::invalid_argument("Capture verification requires a bounded --frames run");
	}
	if ((O.VerifyClear || O.VerifyTriangle || O.VerifyUi || O.VerifyModel) && O.Capture.empty())
	{
		throw std::invalid_argument("Verification requires --capture");
	}
	std::sort(O.RdcFrames.begin(), O.RdcFrames.end());
	for (std::size_t I = 0; I < O.RdcFrames.size(); ++I)
	{
		if (O.RdcFrames[I] < 1 || !O.Frames || O.RdcFrames[I] + int(O.ExerciseRdcUi) > O.Frames ||
		    (I && O.RdcFrames[I] <= O.RdcFrames[I - 1] + int(O.ExerciseRdcUi)))
		{
			throw std::invalid_argument("--capture-rdc requires distinct positive frame numbers within --frames (UI "
			                            "exercise needs one release frame)");
		}
	}
	if (O.ExerciseRdcUi && (O.RdcFrames.empty() || O.NoUi))
	{
		throw std::invalid_argument("--exercise-rdc-ui requires --capture-rdc and the debug UI");
	}
	return O;
}

void Verify(const Hyperion::FImage& InImage, const Hyperion::FAppSettings& InSettings, const FOptions& InOptions)
{
	if (InOptions.VerifyModel)
	{
		std::size_t Colored{};
		for (std::size_t Index = 0; Index < InImage.Rgba.size(); Index += 4)
		{
			if (std::max({InImage.Rgba[Index], InImage.Rgba[Index + 1], InImage.Rgba[Index + 2]}) > .22f)
			{
				++Colored;
			}
		}
		if (Colored < std::size_t(InImage.Width) * InImage.Height / 40)
		{
			throw std::runtime_error("Model readback has insufficient visible geometry");
		}
	}
	if (InOptions.VerifyClear)
	{
		for (std::size_t I = 0; I < InImage.Rgba.size(); I += 4)
		{
			if (std::abs(InImage.Rgba[I] - InSettings.ClearRed) > 1.0 / 255 ||
			    std::abs(InImage.Rgba[I + 1] - InSettings.ClearGreen) > 1.0 / 255 ||
			    std::abs(InImage.Rgba[I + 2] - InSettings.ClearBlue) > 1.0 / 255)
			{
				throw std::runtime_error("Clear readback mismatch");
			}
		}
	}
	if (InOptions.VerifyTriangle)
	{
		auto Center = (std::size_t(InImage.Height / 2) * InImage.Width + InImage.Width / 2) * 4;
		auto Corner = (std::size_t(10) * InImage.Width + 10) * 4;
		if (InImage.Rgba[Center] < .15f || InImage.Rgba[Center + 1] < .15f || InImage.Rgba[Center + 2] < .15f ||
		    std::abs(InImage.Rgba[Corner] - InSettings.ClearRed) > 1.0 / 255)
		{
			throw std::runtime_error("Triangle readback mismatch");
		}
	}
	if (InOptions.VerifyUi)
	{
		std::size_t Bright{};
		for (std::uint32_t Y = 30; Y < std::min(InImage.Height, 650u); ++Y)
		{
			for (std::uint32_t X = 30; X < std::min(InImage.Width, 340u); ++X)
			{
				auto I = (std::size_t(Y) * InImage.Width + X) * 4;
				if (InImage.Rgba[I] > .3f && InImage.Rgba[I + 1] > .3f)
				{
					++Bright;
				}
			}
		}
		if (Bright < 500)
		{
			throw std::runtime_error("GUI text/controls missing from GPU readback");
		}
	}
}
} // namespace

namespace
{
#if HYP_ENABLE_RENDERDOC
struct FFrameCaptureScope
{
	Hyperion::FTaskSystem& Tasks;
	Hyperion::FFrameCapture* Capture;
	bool Active = false;

	FFrameCaptureScope(Hyperion::FTaskSystem& InTasks, Hyperion::FFrameCapture* InCapture,
	                   Hyperion::FNativeSurface InSurface)
	    : Tasks(InTasks), Capture(InCapture)
	{
		if (Capture)
		{
			Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
			                          [&]
			                          {
				                          Active = Capture->BeginFrame(InSurface);
			                          }));
		}
	}

	~FFrameCaptureScope()
	{
		if (Active)
		{
			try
			{
				Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
				                          [&]
				                          {
					                          Capture->Cancel();
				                          }));
			}
			catch (...)
			{
			}
		}
	}

	bool Finish()
	{
		bool Success = false;
		if (Active)
		{
			Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
			                          [&]
			                          {
				                          Success = Capture->EndFrame();
			                          }));
			Active = false;
		}
		return Success;
	}
};
#endif
// Join every file producer on exception paths too, before IO and Tasks are destroyed.
struct FFileWriteScope
{
	Hyperion::FTaskSystem& Tasks;
	const std::vector<Hyperion::FTaskHandle>& Writes;

	~FFileWriteScope()
	{
		for (const auto& Write : Writes)
		{
			try
			{
				Tasks.Wait(Write);
			}
			catch (...)
			{
				// The normal path propagates save failures; unwinding must still drain all jobs.
			}
		}
	}
};
} // namespace

int main(int InArgCount, char** InArgValues)
{
	try
	{
		const auto Options = Parse(InArgCount, InArgValues);
		Hyperion::InitializeLog(std::filesystem::path(HYP_SOURCE_DIR) / "out/logs/viewer.log");
		auto Settings = Hyperion::LoadSettings(Options.Config);
		if (!Options.Model.empty())
		{
			Settings.ModelSource = Options.Model.string();
		}
		if (!Settings.ModelSource.empty())
		{
			std::erase(Settings.Plugins, std::string("triangle"));
			if (std::find(Settings.Plugins.begin(), Settings.Plugins.end(), "model-viewer") == Settings.Plugins.end())
			{
				Settings.Plugins.insert(Settings.Plugins.begin(), "model-viewer");
			}
		}
		if (Options.Backend)
		{
			Settings.RHIBackend = *Options.Backend;
		}
		if (Options.RenderDocLibrary)
		{
			Settings.RenderDocLibrary = *Options.RenderDocLibrary;
		}
		if (Options.RdcOutput)
		{
			Settings.RenderDocOutput = *Options.RdcOutput;
		}
		Settings.RenderDocAutoOpen = Settings.RenderDocAutoOpen || Options.OpenRdc;
		if (Options.RenderDoc &&
		    std::find(Settings.Plugins.begin(), Settings.Plugins.end(), "renderdoc") == Settings.Plugins.end())
		{
			Settings.Plugins.push_back("renderdoc");
		}
		auto Requested = Options.VerifyClear ? std::vector<std::string>{} : Settings.Plugins;
		if (Options.NoUi)
		{
			std::erase(Requested, std::string("debug-ui"));
		}
		const bool EnableRenderDoc =
		    std::find(Settings.Plugins.begin(), Settings.Plugins.end(), "renderdoc") != Settings.Plugins.end();
		std::erase(Requested, std::string("renderdoc"));
		Hyperion::FDebugMetrics Metrics;
#if HYP_ENABLE_RENDERDOC
		std::unique_ptr<Hyperion::FPluginSet> StartupPlugins;
		Hyperion::FFrameCapture* FrameCapture = nullptr;
		Metrics.FrameCapture.Compiled = true;
		Metrics.FrameCapture.Status = "RenderDoc disabled (enable plugin, save and restart)";
		if (EnableRenderDoc)
		{
			Hyperion::FPluginRegistry StartupRegistry;
			Hyperion::RegisterRenderDocPlugin(
			    StartupRegistry,
			    {std::filesystem::path(
			         std::u8string(Settings.RenderDocLibrary.begin(), Settings.RenderDocLibrary.end())),
			     std::filesystem::path(std::u8string(Settings.RenderDocOutput.begin(), Settings.RenderDocOutput.end())),
			     Settings.ModelSource.empty() ? "Triangle" : "Model"});
			const std::array<std::string, 1> StartupIds{"renderdoc"};
			// This runs before any window or DXGI/D3D12 API can create graphics objects.
			StartupPlugins = std::make_unique<Hyperion::FPluginSet>(StartupRegistry.Activate(StartupIds));
			for (const auto& Plugin : StartupPlugins->GetInstances())
			{
				FrameCapture = &static_cast<Hyperion::FRenderDocPlugin&>(*Plugin).Capture();
			}
		}
#else
		if (EnableRenderDoc)
		{
			throw std::runtime_error("RenderDoc plugin is not compiled; configure HYP_ENABLE_RENDERDOC=ON");
		}
#endif
		const auto SelectedBackend = Hyperion::ParseRHIBackend(Settings.RHIBackend);
		Hyperion::FRHIBackendRegistry Backends;
		Hyperion::RegisterD3D12RHIBackend(Backends);
		if (!Backends.IsRegistered(SelectedBackend))
		{
			throw std::runtime_error("RHI backend is not registered: " + Settings.RHIBackend);
		}
		Hyperion::FTaskSystem Tasks(static_cast<unsigned>(Settings.Workers),
		                            static_cast<unsigned>(Settings.RhiThreads));
		Hyperion::FIOService IO(Tasks);
		Hyperion::FAssetService Assets(IO);
		Hyperion::RegisterGltfImporter(Assets);
		std::vector<Hyperion::FTaskHandle> FileWrites;
		FFileWriteScope FileWriteScope{Tasks, FileWrites};
		const auto SaveSettingsAsync = [&](const std::filesystem::path& InPath)
		{
			const auto Text = Hyperion::EncodeReflected(Hyperion::SettingsType(), &Settings);
			const auto Bytes = std::as_bytes(std::span(Text));
			FileWrites.push_back(IO.WriteAsync(InPath, {Bytes.begin(), Bytes.end()}).Task());
		};
		Hyperion::FWindow Window(Settings.Title,
		                         {static_cast<unsigned>(Settings.Width), static_cast<unsigned>(Settings.Height)},
		                         Options.Hidden);
		std::unique_ptr<Hyperion::IRHIDevice> Device;
		std::unique_ptr<Hyperion::IRHISwapchain> Swapchain;
		const auto Surface = Window.Surface();
		const auto InitialSize = Window.PixelSize();
		Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Hyperion::FRHIDeviceDesc DeviceDesc;
			                          DeviceDesc.RequiredFeatures = {Hyperion::ERHIFeature::Graphics,
			                                                         Hyperion::ERHIFeature::TextureSampling};
			                          Device = Backends.CreateDevice(SelectedBackend, DeviceDesc);
			                          Swapchain = Device->CreateSwapchain({Surface, InitialSize});
		                          }));
		Hyperion::FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
		                                   std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache");
		std::unique_ptr<Hyperion::FGui> Gui;
		Hyperion::FImage Font;
		if (std::find(Requested.begin(), Requested.end(), "debug-ui") != Requested.end())
		{
			Gui = std::make_unique<Hyperion::FGui>(&Window);
			Font = Gui->FontImage();
		}
		Hyperion::FPluginRegistry Registry;
		Hyperion::RegisterTrianglePlugin(Registry, *Device, Compiler, Tasks);
		Hyperion::RegisterModelViewerPlugin(Registry, *Device, Compiler, Tasks, Assets, Settings.ModelSource);
		Hyperion::RegisterDebugUiPlugin(Registry, *Device, Compiler, Tasks, Font);
		std::unique_ptr<Hyperion::FPluginSet> Plugins;
		Hyperion::FDebugUiPlugin* GuiPlugin{};
		Hyperion::FModelViewerPlugin* ModelPlugin{};
		Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Plugins = std::make_unique<Hyperion::FPluginSet>(Registry.Activate(Requested));
			                          for (const auto& Plugin : Plugins->GetInstances())
			                          {
				                          if (auto Debug = dynamic_cast<Hyperion::FDebugUiPlugin*>(Plugin.get()))
				                          {
					                          GuiPlugin = Debug;
				                          }
				                          if (auto Model = dynamic_cast<Hyperion::FModelViewerPlugin*>(Plugin.get()))
				                          {
					                          ModelPlugin = Model;
				                          }
			                          }
			                          Metrics.Device = Device->Statistics();
		                          }));
		Font = {};
		Hyperion::Log(Hyperion::ELogLevel::Info, "Windows rendering application started");
		auto LastFrame = Hyperion::ClockNanoseconds();
		bool Captured = false;
		Hyperion::FVec4 RdcButtonBounds;
		bool RdcMouseDown = false;
		if (Options.ExerciseRdcUi && (!Gui || !Settings.ShowGui))
		{
			throw std::runtime_error("RDC UI exercise requires a visible diagnostics panel");
		}
		for (int Frame = 0; !Window.ShouldClose() && (!Options.Frames || Frame < Options.Frames); ++Frame)
		{
			Hyperion::FProfileScope Scope("Application frame");
			auto Now = Hyperion::ClockNanoseconds();
			float Delta = Frame ? float(Now - LastFrame) / 1e9f : 1.f / 60.f;
			LastFrame = Now;
			Window.Poll();
			Tasks.PumpMain();
			if (ModelPlugin)
			{
				Metrics.AssetStatus = ModelPlugin->Status();
				for (const auto& Event : Window.Events())
				{
					if (Event.Type == Hyperion::EEventType::Key && Event.Key == Hyperion::EKey::Tab && Event.Down)
					{
						Settings.ShowGui = !Settings.ShowGui;
					}
				}
			}
			if (Options.Exercise)
			{
				if (Frame == 2)
				{
					Window.Resize({960, 540});
				}
				if (Frame == 4)
				{
					Window.Minimize();
				}
				if (Frame == 6)
				{
					Window.Restore();
				}
			}
			const auto Size = Window.PixelSize();
			const auto Logical = Window.LogicalSize();
			if (Window.Minimized() || !Size.Width || !Size.Height)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			Metrics.Threads = Tasks.Statistics();
			Metrics.FrameMilliseconds.push_back(Delta * 1000);
			if (Metrics.FrameMilliseconds.size() > 120)
			{
				Metrics.FrameMilliseconds.erase(Metrics.FrameMilliseconds.begin());
			}
			Hyperion::FGuiDrawData GuiData;
			Hyperion::FDebugActions Actions;
			const bool ScheduledRdc = std::binary_search(Options.RdcFrames.begin(), Options.RdcFrames.end(), Frame + 1);
#if HYP_ENABLE_RENDERDOC
			if (FrameCapture)
			{
				const auto CaptureStatus = FrameCapture->Status();
				Metrics.FrameCapture.Available = CaptureStatus.Available;
				Metrics.FrameCapture.Busy = CaptureStatus.State == Hyperion::EFrameCaptureState::Pending ||
				                            CaptureStatus.State == Hyperion::EFrameCaptureState::Capturing;
				Metrics.FrameCapture.Status = CaptureStatus.Message;
				const auto Path = CaptureStatus.LastCapture.u8string();
				Metrics.FrameCapture.LastCapture.assign(reinterpret_cast<const char*>(Path.data()), Path.size());
				Metrics.FrameCapture.OpenStatus = CaptureStatus.ReplayMessage;
			}
#endif
			if (Gui && Settings.ShowGui)
			{
				std::vector<Hyperion::FInputEvent> UiEvents(Window.Events().begin(), Window.Events().end());
				if (Options.ExerciseRdcUi && (ScheduledRdc || RdcMouseDown))
				{
					Hyperion::FInputEvent Move;
					Move.Type = Hyperion::EEventType::MouseMove;
					Move.X = (RdcButtonBounds.X + RdcButtonBounds.Z) * .5f;
					Move.Y = (RdcButtonBounds.Y + RdcButtonBounds.W) * .5f;
					Hyperion::FInputEvent Button;
					Button.Type = Hyperion::EEventType::MouseButton;
					Button.Down = ScheduledRdc;
					UiEvents.push_back(Move);
					UiEvents.push_back(Button);
					RdcMouseDown = ScheduledRdc;
				}
				Gui->BeginFrame(Logical, Size, Delta, UiEvents);
				Actions = Hyperion::DrawDebugPanel(*Gui, Settings, Metrics, Logical);
				RdcButtonBounds = Actions.CaptureRdcBounds;
				GuiData = Gui->Render();
			}
#if HYP_ENABLE_RENDERDOC
			if (FrameCapture && (Actions.CaptureRdc || (ScheduledRdc && !Options.ExerciseRdcUi)))
			{
				FrameCapture->RequestCapture();
			}
			if (FrameCapture && Actions.OpenRdc)
			{
				FrameCapture->OpenLastCapture();
			}
			bool RdcSucceeded = false;
#endif
			if (Actions.Save)
			{
				SaveSettingsAsync(Options.Config);
				Hyperion::Log(Hyperion::ELogLevel::Info, "Experiment save queued: " + Options.Config.string());
			}
			if (ModelPlugin)
			{
				ModelPlugin->Input(Window.Events(), Gui && Settings.ShowGui && Gui->WantsMouse(),
				                   Gui && Settings.ShowGui && Gui->WantsKeyboard());
			}
			const bool TakeCapture = Actions.Capture || (!Options.Capture.empty() && Frame == Options.Frames - 1);
			Hyperion::FImage Screenshot;
			Tasks.Wait(Tasks.Dispatch(
			    {Hyperion::EDomain::Render},
			    [&]
			    {
				    Hyperion::FProfileScope Prepare("Render preparation");
#if HYP_ENABLE_RENDERDOC
				    FFrameCaptureScope CaptureScope(Tasks, FrameCapture, Surface);
#endif
				    if (GuiPlugin)
				    {
					    Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
					                              [&]
					                              {
						                              GuiPlugin->Prepare(GuiData);
					                              }));
				    }
				    Hyperion::FRenderGraph Graph;
				    Hyperion::FColorPass Clear;
				    Clear.Commands.Name = "Clear";
				    Clear.Load = Hyperion::EColorLoad::Clear;
				    Clear.Commands.ClearColor = {float(Settings.ClearRed), float(Settings.ClearGreen),
				                                 float(Settings.ClearBlue), 1};
				    Graph.Add(std::move(Clear));
				    for (const auto& Plugin : Plugins->GetInstances())
				    {
					    if (auto Render = dynamic_cast<Hyperion::IRenderPlugin*>(Plugin.get()))
					    {
						    Render->Build(Graph, {Size, Settings});
					    }
				    }
				    Screenshot = Hyperion::ExecuteGraph(Graph, Tasks, *Swapchain, Size, Settings.Vsync, TakeCapture);
#if HYP_ENABLE_RENDERDOC
				    RdcSucceeded = CaptureScope.Finish();
#endif
				    Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
				                              [&]
				                              {
					                              Metrics.Device = Device->Statistics();
				                              }));
			    }));
#if HYP_ENABLE_RENDERDOC
			if (RdcSucceeded && Settings.RenderDocAutoOpen)
			{
				FrameCapture->OpenLastCapture();
			}
#endif
			if (Metrics.Device.ValidationErrors)
			{
				throw std::runtime_error("RHI validation errors");
			}
			if (TakeCapture)
			{
				if (Options.VerifyModel && (!ModelPlugin || !ModelPlugin->Ready()))
				{
					throw std::runtime_error(ModelPlugin ? ModelPlugin->Status() : "No model plugin active");
				}
				auto Path = Options.Capture.empty()
				                ? std::filesystem::path(HYP_SOURCE_DIR) / "out/captures" /
				                      ("capture-" + std::to_string(Hyperion::ClockNanoseconds()) + ".png")
				                : Options.Capture;
				Verify(Screenshot, Settings, Options);
				FileWrites.push_back(Hyperion::DispatchAsync<bool>(
				                         Tasks, {Hyperion::EDomain::Worker},
				                         [&, Path, Snapshot = std::move(Screenshot)]
				                         {
					                         return *IO.WriteAsync(Path, Hyperion::EncodePng(Snapshot)).Get(Tasks);
				                         })
				                         .Task());
				Captured = true;
				Hyperion::Log(Hyperion::ELogLevel::Info, "Screenshot save queued: " + Path.string());
			}
			Hyperion::ProfileFrame();
		}
		if (!Options.Capture.empty() && !Captured)
		{
			throw std::runtime_error("Requested capture was not produced");
		}
#if HYP_ENABLE_RENDERDOC
		if (!Options.RdcFrames.empty() &&
		    (!FrameCapture || FrameCapture->Status().CompletedCaptures != Options.RdcFrames.size()))
		{
			throw std::runtime_error(
			    "Requested RDC capture was not produced: " +
			    (FrameCapture ? FrameCapture->Status().Message : std::string("plugin unavailable")));
		}
#endif
		if (!Options.SaveConfig.empty())
		{
			auto Size = Window.LogicalSize();
			Settings.Width = static_cast<int>(Size.Width);
			Settings.Height = static_cast<int>(Size.Height);
			SaveSettingsAsync(Options.SaveConfig);
		}
		Assets.Drain();
		Tasks.WaitAll(FileWrites);
		Hyperion::FDeviceStats Stats;
		Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          Plugins.reset();
			                          Swapchain.reset();
			                          Stats = Device->Statistics();
			                          Device.reset();
#if HYP_ENABLE_RENDERDOC
			                          if (StartupPlugins)
			                          {
				                          StartupPlugins->Stop();
			                          }
#endif
		                          }));
		Gui.reset();
		for (const auto& Thread : Tasks.Statistics())
		{
			Hyperion::Log(Hyperion::ELogLevel::Info, "Domain " + Thread.Name + ": " + std::to_string(Thread.Executed) +
			                                             " tasks; thread ID " + std::to_string(Thread.ThreadId));
		}
		Tasks.Shutdown();
		Hyperion::Log(Hyperion::ELogLevel::Info, "GPU frames: " + std::to_string(Stats.SubmittedFrames) +
		                                             "; validation errors: " + std::to_string(Stats.ValidationErrors));
		if (Stats.ValidationErrors)
		{
			throw std::runtime_error("RHI validation errors at shutdown");
		}
		if (Hyperion::MemoryStats(Hyperion::EMemoryTag::Gui).LiveBytes ||
		    Hyperion::MemoryStats(Hyperion::EMemoryTag::Render).LiveBytes)
		{
			throw std::runtime_error("GUI or renderer hooked allocations survived shutdown");
		}
		Hyperion::Log(Hyperion::ELogLevel::Info, "Rendering lifecycle completed successfully");
		Hyperion::ShutdownLog();
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << "Hyperion: " << Error.what() << '\n';
		return 1;
	}
}
