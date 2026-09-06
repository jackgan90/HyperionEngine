#include "Hyperion/RHI/RHIBackend.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <type_traits>

namespace
{
using namespace Hyperion;

template<class F> void Rejects(F InOperation)
{
	bool Failed = false;
	try
	{
		InOperation();
	}
	catch (const std::exception&)
	{
		Failed = true;
	}
	HYP_CHECK(Failed);
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
		++Begun;
	}

	FRecordedList Record(std::uint32_t, const FPassCommands&) override
	{
		++Recorded;
		return {};
	}

	FImage EndFrame(std::span<const FRecordedList> InLists, bool, bool) override
	{
		HYP_CHECK(InLists.size() == Recorded);
		++Submitted;
		return {1, 1, EColorSpace::Linear, {1, 0, 0, 1}};
	}

	void WaitIdle() override
	{
	}

	FRHICapabilities Capabilities;
	std::uint32_t Begun{};
	std::uint32_t Recorded{};
	std::uint32_t Submitted{};
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
	}

	FDeviceStats Statistics() const override
	{
		return {};
	}

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
		HYP_CHECK(Device->QueryFeature(ERHIFeature::RayTracing).Supported);
		HYP_CHECK(!Device->QueryFeature(ERHIFeature::RayTracing).Enabled);
		HYP_CHECK(!Device->QueryFeature(ERHIFeature::MeshShaders).Enabled);
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
		Test.Capabilities.Features[static_cast<std::size_t>(ERHIFeature::Readback)].Enabled = false;
		Rejects(
		    [&]
		    {
			    ExecuteGraph(Graph, Tasks, *Swapchain, {32, 32}, false, true);
		    });
		HYP_CHECK(Test.Begun == 1);
		Tasks.Shutdown();
		std::cout << "Independent RHI provider and renderer contracts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
