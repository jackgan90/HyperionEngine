#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Hyperion/RHI/RHIBackend.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <type_traits>

namespace
{
using namespace Hyperion;

template<class F> void Rejects(F InOperation)
{
	bool bFailed = false;
	try
	{
		InOperation();
	}
	catch (const std::exception&)
	{
		bFailed = true;
	}
	HYP_CHECK(bFailed);
}

// Deliberately implemented using only public contracts; this target never links a native backend.
class FTestSwapchain final : public IRHISwapchain
{
public:
	explicit FTestSwapchain(const FRHICapabilities& InCapabilities) : Capabilities(InCapabilities)
	{
	}

	const FRHICapabilities& GetCapabilities() const noexcept override
	{
		return Capabilities;
	}

	void BeginFrame(FSize) override
	{
		HYP_CHECK(!bActive);
		bActive = true;
		Recorded = 0;
		++Begun;
	}

	FRecordedList Record(std::uint32_t InContext, const FPassCommands&) override
	{
		if (OnRecord)
		{
			OnRecord(InContext);
		}
		++Recorded;
		return {};
	}

	FImage EndFrame(std::span<const FRecordedList> InLists, bool, bool) override
	{
		HYP_CHECK(InLists.size() == Recorded);
		if (bFailEnd)
		{
			throw std::runtime_error("injected submission failure");
		}
		bActive = false;
		++Submitted;
		return {1, 1, EColorSpace::Linear, {1, 0, 0, 1}};
	}

	void WaitIdle() override
	{
	}

	void CancelFrame() override
	{
		if (OnCancel)
		{
			OnCancel();
		}
		bActive = false;
		++Cancelled;
	}

	FRHICapabilities Capabilities;
	std::function<void(std::uint32_t)> OnRecord;
	std::function<void()> OnCancel;
	bool bActive{};
	bool bFailEnd{};
	std::uint32_t Begun{};
	std::atomic<std::uint32_t> Recorded{};
	std::uint32_t Submitted{};
	std::uint32_t Cancelled{};
};

class FTestDevice final : public IRHIDevice
{
public:
	FTestDevice()
	{
		Capabilities.Backend = ERHIBackend::Vulkan; // Test identity only, no claim of a Vulkan implementation.
		Capabilities.ShaderFormat = EShaderFormat::Spirv;
		Capabilities.MaxRecordingContexts = 2;
		Capabilities.Features[static_cast<std::size_t>(ERHIFeature::Graphics)] = {true, true};
		Capabilities.Features[static_cast<std::size_t>(ERHIFeature::Readback)] = {true, true};
		Capabilities.Features[static_cast<std::size_t>(ERHIFeature::RayTracing)] = {true, false};
	}

	const FRHICapabilities& GetCapabilities() const noexcept override
	{
		return Capabilities;
	}

	FRHIFeatureSupport QueryFeature(ERHIFeature InFeature) const override
	{
		return Capabilities.QueryFeature(InFeature);
	}

	FBuffer CreateBuffer(std::span<const std::byte>) override
	{
		return {};
	}

	FTexture CreateTexture(const FImage&) override
	{
		return {};
	}

	FPipeline CreatePipeline(const FPipelineDesc&) override
	{
		return {};
	}

	std::unique_ptr<IRHISwapchain> CreateSwapchain(const FRHISwapchainDesc&) override
	{
		return std::make_unique<FTestSwapchain>(Capabilities);
	}

	void WaitIdle() override
	{
		if (bFailNextIdle)
		{
			bFailNextIdle = false;
			throw std::runtime_error("injected one-time WaitIdle failure");
		}
	}

	FDeviceStats Statistics() const override
	{
		return {};
	}

	bool bFailNextIdle{};

private:
	FRHICapabilities Capabilities;
};

class FTestBackend final : public IRHIBackend
{
public:
	ERHIBackend GetBackend() const noexcept override
	{
		return ERHIBackend::Vulkan;
	}

	std::unique_ptr<IRHIDevice> CreateDevice(const FRHIDeviceDesc&) override
	{
		return std::make_unique<FTestDevice>();
	}
};

void CheckFrameErrors(const FRHICapabilities& InCapabilities)
{
	FTestSwapchain Swapchain(InCapabilities);
	Swapchain.Capabilities.Features[static_cast<std::size_t>(ERHIFeature::ConcurrentRecording)] = {true, true};
	FTaskSystem Tasks(1, 2);
	FRenderGraph Graph;
	FColorPass Clear;
	Clear.Commands.Name = "clear";
	Clear.Load = EColorLoad::Clear;
	Graph.Add(Clear);
	std::atomic<bool> bPeerStarted{};
	std::atomic<bool> bPeerFinished{};
	Swapchain.OnRecord = [&](std::uint32_t InContext)
	{
		if (InContext == 0)
		{
			while (!bPeerStarted)
			{
				std::this_thread::yield();
			}
			throw std::runtime_error("injected recording failure");
		}
		bPeerStarted = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		bPeerFinished = true;
	};
	Swapchain.OnCancel = [&]
	{
		HYP_CHECK(bPeerFinished);
	};
	const auto Execute = [&]
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          ExecuteGraph(Graph, Tasks, Swapchain, {32, 32}, false, false);
		                          }));
	};
	Rejects(Execute);
	HYP_CHECK(bPeerFinished && Swapchain.Cancelled == 1 && !Swapchain.bActive && Swapchain.Submitted == 0);
	Swapchain.OnRecord = {};
	Execute();
	HYP_CHECK(Swapchain.Submitted == 1);
	Swapchain.bFailEnd = true;
	Rejects(Execute);
	HYP_CHECK(Swapchain.Cancelled == 2 && !Swapchain.bActive);
	Swapchain.bFailEnd = false;
	Execute();
	HYP_CHECK(Swapchain.Submitted == 2);
	Tasks.Shutdown();
}

void CheckFrameCoordinator(const FRHICapabilities& InCapabilities)
{
	FTaskSystem Tasks(1, 2);
	FTestSwapchain Swapchain(InCapabilities);
	Swapchain.Capabilities.MaxRecordingContexts = 8;
	Swapchain.Capabilities.Features[static_cast<std::size_t>(ERHIFeature::ConcurrentRecording)] = {true, true};
	unsigned Stage{};
	bool bFailPreparation = false;
	unsigned Failures{};
	FRenderGraphCallbacks Callbacks;
	Callbacks.BeforePrepare = [&]
	{
		Tasks.Require({EDomain::Rhi, 0});
		HYP_CHECK(Stage == 0 && !Swapchain.bActive);
		Stage = 1;
	};
	Callbacks.AfterSubmit = [&]
	{
		Tasks.Require({EDomain::Rhi, 0});
		HYP_CHECK(Stage == 2 && !Swapchain.bActive && Swapchain.Recorded == 6);
		Stage = 3;
	};
	Callbacks.OnFailure = [&]
	{
		Tasks.Require({EDomain::Rhi, 0});
		++Failures;
	};
	Swapchain.OnRecord = [&](std::uint32_t InContext)
	{
		Tasks.Require({EDomain::Rhi, InContext % 2});
		HYP_CHECK(Stage == 2);
	};
	const auto Execute = [&]
	{
		Stage = 0;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          FRenderGraph Graph;
			                          Graph.AddDeferred(
			                              [&]
			                              {
				                              Tasks.Require({EDomain::Rhi, 0});
				                              HYP_CHECK(Stage == 1 && !Swapchain.bActive);
				                              if (bFailPreparation)
				                              {
					                              throw std::runtime_error("injected deferred preparation failure");
				                              }
				                              std::vector<FColorPass> Passes(5);
				                              Passes[0].Load = EColorLoad::Clear;
				                              for (std::size_t Index = 0; Index < Passes.size(); ++Index)
				                              {
					                              Passes[Index].Commands.Name = std::to_string(Index);
				                              }
				                              Stage = 2;
				                              return Passes;
			                              });
			                          ExecuteGraph(std::move(Graph), Tasks, Swapchain, {32, 32}, false, false,
			                                       Callbacks);
		                          }));
	};
	Execute();
	HYP_CHECK(Stage == 3 && Swapchain.Submitted == 1);
	bFailPreparation = true;
	Rejects(Execute);
	HYP_CHECK(Failures == 1 && Swapchain.Begun == 1 && Swapchain.Cancelled == 0);
	bFailPreparation = false;
	Execute();
	HYP_CHECK(Stage == 3 && Swapchain.Submitted == 2);
}

void CheckDeferredOwnerLifetime(bool bInDestroy, bool bInDepthPreview)
{
	FTaskSystem Tasks(1, 2);
	FTestDevice Device;
	FTestSwapchain Swapchain(Device.GetCapabilities());
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "deferred-owner-shader-cache");
	auto Session = std::make_unique<FRenderSession>(Tasks, Device, Compiler);
	FRenderGraph Graph;
	const auto Frame = Session->FreezeFrame(0);
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          if (bInDepthPreview)
		                          {
			                          auto Source =
			                              std::make_shared<const FMaterialTextureSource>(FMaterialDepthTexture{32, 32});
			                          Session->AppendDepthPreview(Graph, Source,
			                                                      Session->GetResources().CreateScopeLifetime(),
			                                                      {0, 0, 32, 32}, true);
		                          }
		                          else
		                          {
			                          FRenderView View;
			                          View.ClearColor = FVec4{};
			                          Session->BuildViews(Graph, std::span(&View, 1), Frame, 1, false, true);
		                          }
	                          }));
	Session->Close();
	if (bInDestroy)
	{
		Session.reset();
	}
	Rejects(
	    [&]
	    {
		    ExecuteGraph(std::move(Graph), Tasks, Swapchain, {32, 32}, false, false);
	    });
	HYP_CHECK(Swapchain.Begun == 0); // Owner failure is reported before acquiring a GPU frame.
}

void CheckSessionCloseRetry()
{
	FTaskSystem Tasks(1, 2);
	FTestDevice Device;
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "close-retry-shader-cache");
	auto Session = std::make_unique<FRenderSession>(Tasks, Device, Compiler);
	Device.bFailNextIdle = true;
	Rejects(
	    [&]
	    {
		    Session->Close();
	    });
	Session->Close();
	Session->Close();
	Session.reset();
}

void CheckDeferredGuiOwner(bool bInDestroy)
{
	FTaskSystem Tasks(1, 2);
	FTestDevice Device;
	FTestSwapchain Swapchain(Device.GetCapabilities());
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "deferred-gui-shader-cache");
	auto Plugin = std::make_unique<FDebugUiPlugin>(Device, Compiler, Tasks, FImage{});
	FRenderGraph Graph;
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          FGuiDrawData Data;
		                          Data.Commands.resize(1);
		                          Plugin->BuildDeferred(Graph, std::move(Data));
	                          }));
	Plugin->Stop();
	if (bInDestroy)
	{
		Plugin.reset();
	}
	Rejects(
	    [&]
	    {
		    ExecuteGraph(std::move(Graph), Tasks, Swapchain, {32, 32}, false, false);
	    });
	HYP_CHECK(Swapchain.Begun == 0);
}
} // namespace

int main()
{
	try
	{
		static_assert(std::is_abstract_v<IRHIDevice> && std::is_abstract_v<IRHISwapchain>);
		static_assert(std::has_virtual_destructor_v<IRHIDevice> && std::has_virtual_destructor_v<IRHIResource>);
		HYP_CHECK(ParseRHIBackend("d3d12") == ERHIBackend::D3D12);
		HYP_CHECK(ParseRHIBackend("metal") == ERHIBackend::Metal);
		Rejects(
		    []
		    {
			    ParseRHIBackend("typo");
		    });
		std::unique_ptr<IRHIDevice> Device;
		{
			FRHIBackendRegistry Registry;
			Rejects(
			    [&]
			    {
				    Registry.CreateDevice(ERHIBackend::Vulkan);
			    });
			Rejects(
			    [&]
			    {
				    Registry.Register(nullptr);
			    });
			Registry.Register(std::make_unique<FTestBackend>());
			Rejects(
			    [&]
			    {
				    Registry.Register(std::make_unique<FTestBackend>());
			    });
			HYP_CHECK(Registry.IsRegistered(ERHIBackend::Vulkan) && !Registry.IsRegistered(ERHIBackend::D3D12));
			FRHIDeviceDesc Desc;
			Desc.RequiredFeatures = {ERHIFeature::Graphics};
			Desc.OptionalFeatures = {ERHIFeature::MeshShaders};
			Device = Registry.CreateDevice(ERHIBackend::Vulkan, Desc);
			Desc.RequiredFeatures = {ERHIFeature::RayTracing};
			Rejects(
			    [&]
			    {
				    Registry.CreateDevice(ERHIBackend::Vulkan, Desc);
			    });
		}
		HYP_CHECK(Device->GetCapabilities().ShaderFormat == EShaderFormat::Spirv);
		HYP_CHECK(Device->QueryFeature(ERHIFeature::RayTracing).bSupported);
		HYP_CHECK(!Device->QueryFeature(ERHIFeature::RayTracing).bEnabled);
		HYP_CHECK(!Device->QueryFeature(ERHIFeature::MeshShaders).bEnabled);
		Rejects(
		    [&]
		    {
			    Device->QueryFeature(ERHIFeature::Count);
		    });

		auto Swapchain = Device->CreateSwapchain({});
		auto& Test = dynamic_cast<FTestSwapchain&>(*Swapchain);
		FTaskSystem Tasks(1, 2);
		FRenderGraph Graph;
		FColorPass Clear;
		Clear.Commands.Name = "clear";
		Clear.Load = EColorLoad::Clear;
		Graph.Add(Clear);
		auto Image = ExecuteGraph(Graph, Tasks, *Swapchain, {32, 32}, false, true);
		HYP_CHECK(Image.Width == 1 && Test.Submitted == 1 && Test.Recorded == 2);
		Test.Capabilities.MaxRecordingContexts = 1;
		Rejects(
		    [&]
		    {
			    ExecuteGraph(Graph, Tasks, *Swapchain, {32, 32}, false, false);
		    });
		HYP_CHECK(Test.Begun == 1); // Capacity failure must not acquire a frame.
		Test.Capabilities.MaxRecordingContexts = 2;
		Test.Capabilities.Features[static_cast<std::size_t>(ERHIFeature::Readback)].bEnabled = false;
		Rejects(
		    [&]
		    {
			    ExecuteGraph(Graph, Tasks, *Swapchain, {32, 32}, false, true);
		    });
		HYP_CHECK(Test.Begun == 1);
		Tasks.Shutdown();
		CheckFrameErrors(Device->GetCapabilities());
		CheckFrameCoordinator(Device->GetCapabilities());
		for (const bool bDestroy : {false, true})
		{
			CheckDeferredOwnerLifetime(bDestroy, false);
			CheckDeferredOwnerLifetime(bDestroy, true);
			CheckDeferredGuiOwner(bDestroy);
		}
		CheckSessionCloseRetry();
		std::cout << "Independent RHI provider and renderer contracts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
