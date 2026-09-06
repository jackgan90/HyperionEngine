#include "hyperion/Core.h"
#include "hyperion/DebugUI.h"
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
	int Frames{};
	bool Hidden{};
	bool Exercise{};
	bool VerifyClear{};
	bool VerifyTriangle{};
	bool VerifyUi{};
	bool NoUi{};
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
		else if (Arg == "--save-config" && I + 1 < InArgc)
		{
			O.SaveConfig = InArgv[++I];
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
		else
		{
			throw std::invalid_argument("Unknown or incomplete option: " + Arg);
		}
	}
	if (O.Frames < 0)
	{
		throw std::invalid_argument("--frames must be nonnegative");
	}
	if ((!O.Capture.empty() || O.VerifyClear || O.VerifyTriangle || O.VerifyUi) && O.Frames == 0)
	{
		throw std::invalid_argument("Capture verification requires a bounded --frames run");
	}
	if ((O.VerifyClear || O.VerifyTriangle || O.VerifyUi) && O.Capture.empty())
	{
		throw std::invalid_argument("Verification requires --capture");
	}
	return O;
}

void Verify(const Hyperion::FImage& InImage, const Hyperion::FAppSettings& InSettings, const FOptions& InOptions)
{
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

int main(int InArgCount, char** InArgValues)
{
	try
	{
		const auto Options = Parse(InArgCount, InArgValues);
		Hyperion::InitializeLog(std::filesystem::path(HYP_SOURCE_DIR) / "out/logs/viewer.log");
		auto Settings = Hyperion::LoadSettings(Options.Config);
		Hyperion::FTaskSystem Tasks(static_cast<unsigned>(Settings.Workers),
		                            static_cast<unsigned>(Settings.RhiThreads));
		Hyperion::FWindow Window(Settings.Title,
		                         {static_cast<unsigned>(Settings.Width), static_cast<unsigned>(Settings.Height)},
		                         Options.Hidden);
		std::unique_ptr<Hyperion::FRhiDevice> Device;
		const auto Surface = Window.Surface();
		const auto InitialSize = Window.PixelSize();
		Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device = std::make_unique<Hyperion::FRhiDevice>(Surface, InitialSize);
		                          }));
		Hyperion::FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
		                                   std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache");
		auto Requested = Options.VerifyClear ? std::vector<std::string>{} : Settings.Plugins;
		if (Options.NoUi)
		{
			std::erase(Requested, std::string("debug-ui"));
		}
		std::unique_ptr<Hyperion::FGui> Gui;
		Hyperion::FImage Font;
		if (std::find(Requested.begin(), Requested.end(), "debug-ui") != Requested.end())
		{
			Gui = std::make_unique<Hyperion::FGui>(&Window);
			Font = Gui->FontImage();
		}
		Hyperion::FPluginRegistry Registry;
		Hyperion::RegisterTrianglePlugin(Registry, *Device, Compiler, Tasks);
		Hyperion::RegisterDebugUiPlugin(Registry, *Device, Compiler, Tasks, Font);
		std::unique_ptr<Hyperion::FPluginSet> Plugins;
		Hyperion::FDebugUiPlugin* GuiPlugin{};
		Hyperion::FDebugMetrics Metrics;
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
			                          }
			                          Metrics.Device = Device->Statistics();
		                          }));
		Font = {};
		Hyperion::Log(Hyperion::ELogLevel::Info, "Windows rendering application started");
		auto LastFrame = Hyperion::ClockNanoseconds();
		bool Captured = false;
		for (int Frame = 0; !Window.ShouldClose() && (!Options.Frames || Frame < Options.Frames); ++Frame)
		{
			Hyperion::FProfileScope Scope("Application frame");
			auto Now = Hyperion::ClockNanoseconds();
			float Delta = Frame ? float(Now - LastFrame) / 1e9f : 1.f / 60.f;
			LastFrame = Now;
			Window.Poll();
			Tasks.PumpMain();
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
			if (Gui && Settings.ShowGui)
			{
				Gui->BeginFrame(Logical, Size, Delta, Window.Events());
				Actions = Hyperion::DrawDebugPanel(*Gui, Settings, Metrics, Logical);
				GuiData = Gui->Render();
			}
			if (Actions.Save)
			{
				Hyperion::SaveSettings(Options.Config, Settings);
				Hyperion::Log(Hyperion::ELogLevel::Info, "Experiment saved: " + Options.Config.string());
			}
			const bool TakeCapture = Actions.Capture || (!Options.Capture.empty() && Frame == Options.Frames - 1);
			Hyperion::FImage Screenshot;
			Tasks.Wait(Tasks.Dispatch(
			    {Hyperion::EDomain::Render},
			    [&]
			    {
				    Hyperion::FProfileScope Prepare("Render preparation");
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
				    Screenshot = Hyperion::ExecuteGraph(Graph, Tasks, *Device, Size, Settings.Vsync, TakeCapture);
				    Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
				                              [&]
				                              {
					                              Metrics.Device = Device->Statistics();
				                              }));
			    }));
			if (Metrics.Device.ValidationErrors)
			{
				throw std::runtime_error("D3D12 validation errors");
			}
			if (TakeCapture)
			{
				auto Path = Options.Capture.empty()
				                ? std::filesystem::path(HYP_SOURCE_DIR) / "out/captures" /
				                      ("capture-" + std::to_string(Hyperion::ClockNanoseconds()) + ".png")
				                : Options.Capture;
				Hyperion::SaveImage(Path, Screenshot);
				Verify(Screenshot, Settings, Options);
				Captured = true;
				Hyperion::Log(Hyperion::ELogLevel::Info, "Screenshot saved: " + Path.string());
			}
			Hyperion::ProfileFrame();
		}
		if (!Options.Capture.empty() && !Captured)
		{
			throw std::runtime_error("Requested capture was not produced");
		}
		if (!Options.SaveConfig.empty())
		{
			auto Size = Window.LogicalSize();
			Settings.Width = static_cast<int>(Size.Width);
			Settings.Height = static_cast<int>(Size.Height);
			Hyperion::SaveSettings(Options.SaveConfig, Settings);
		}
		Hyperion::FDeviceStats Stats;
		Tasks.Wait(Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          Plugins.reset();
			                          Stats = Device->Statistics();
			                          Device.reset();
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
			throw std::runtime_error("D3D12 validation errors at shutdown");
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
