#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Renderer/RenderFeatures.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <optional>

namespace Hyperion
{
namespace
{
class FGraphicsServicesPlugin final : public FPlugin
{
public:
	explicit FGraphicsServicesPlugin(FGraphicsServiceOptions InOptions) : Options(std::move(InOptions))
	{
	}

	void Start(FPluginContext& InContext) override
	{
		Control = &InContext.Require<FApplicationControl>();
		Tasks = &InContext.Require<FTaskSystem>();
		auto& Window = InContext.Require<FWindow>();
		auto& IO = InContext.Require<FIOService>();
		FRHIBackendRegistry Backends;
		Options.RegisterBackends(Backends);
		const auto Backend = ParseRHIBackend(Options.Backend);
		if (!Backends.IsRegistered(Backend))
		{
			throw std::runtime_error("RHI backend is not registered: " + Options.Backend);
		}
		const auto Surface = Window.Surface();
		const auto Size = Window.PixelSize();
		Tasks->Wait(Tasks->Dispatch({EDomain::Rhi, 0},
		                            [&]
		                            {
			                            FRHIDeviceDesc Desc;
			                            Desc.RequiredFeatures = {ERHIFeature::Graphics, ERHIFeature::TextureSampling};
			                            Device = Backends.CreateDevice(Backend, Desc);
			                            Swapchain = Device->CreateSwapchain(
			                                {Surface, Size, ERHIDepthFormat::D32, Options.bReversedZ ? 0.f : 1.f});
			                            Swapchain->SetGpuTimingEnabled(true);
			                            if (Options.TimingFrames)
			                            {
				                            Device->BeginGpuTimingCapture(Options.TimingFrames);
			                            }
		                            }));
		Compiler = std::make_unique<FShaderCompiler>("/Engine/Shaders", Options.ShaderCache, IO.FileSystem());
		Session = std::make_unique<FRenderSession>(*Tasks, *Device, *Compiler);
		InContext.Provide<IRHIDevice>(*Device);
		InContext.Provide<IRHISwapchain>(*Swapchain);
		InContext.Provide(*Compiler);
		InContext.Provide(*Session);
		InContext.Provide(Features);
	}

	void Stop() noexcept override
	{
		if (Session)
		{
			try
			{
				Session->Close();
			}
			catch (...)
			{
				Control->ReportFailure(std::current_exception());
			}
			Session.reset();
		}
		if (Tasks)
		{
			try
			{
				std::optional<FDeviceStats> FinalStats;
				Tasks->Wait(Tasks->Dispatch({EDomain::Rhi, 0},
				                            [this, &FinalStats]
				                            {
					                            auto FinalDevice = std::move(Device);
					                            auto FinalSwapchain = std::move(Swapchain);
					                            if (FinalDevice)
					                            {
						                            FinalDevice->WaitIdle();
					                            }
					                            FinalSwapchain.reset();
					                            if (FinalDevice)
					                            {
						                            FinalStats = FinalDevice->Statistics();
					                            }
				                            }));
				if (FinalStats)
				{
					Log(ELogLevel::Info,
					    "Final graphics validation errors: " + std::to_string(FinalStats->ValidationErrors));
					if (FinalStats->ValidationErrors)
					{
						throw std::runtime_error("GPU validation errors after graphics shutdown: " +
						                         std::to_string(FinalStats->ValidationErrors));
					}
				}
			}
			catch (const std::exception& Failure)
			{
				Log(ELogLevel::Error, Failure.what());
				Control->ReportFailure(std::current_exception());
			}
		}
		Compiler.reset();
	}

private:
	FGraphicsServiceOptions Options;
	FTaskSystem* Tasks{};
	FApplicationControl* Control{};
	FRenderFeatureRegistry Features;
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FShaderCompiler> Compiler;
	std::unique_ptr<FRenderSession> Session;
};
} // namespace

void RegisterGraphicsServices(FPluginRegistry& InRegistry, FGraphicsServiceOptions InOptions)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "graphics";
	Descriptor.Dependencies = {"assets", "window"};
	Descriptor.After = {"renderdoc"};
	Descriptor.Requires = {typeid(FApplicationControl), typeid(FTaskSystem), typeid(FWindow), typeid(FIOService)};
	Descriptor.Provides = {typeid(IRHIDevice), typeid(IRHISwapchain), typeid(FShaderCompiler), typeid(FRenderSession),
	                       typeid(FRenderFeatureRegistry)};
	Descriptor.Create = [Options = std::move(InOptions)]
	{
		return std::make_unique<FGraphicsServicesPlugin>(Options);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
