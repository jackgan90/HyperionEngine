#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Support/TestSupport.h"
#include <iostream>

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

class FForeignBuffer final : public IRHIBuffer
{
public:
	const void* GetDeviceIdentity() const noexcept override
	{
		return this;
	}
};

FPassCommands ClearCommands()
{
	FPassCommands Commands;
	Commands.Name = "clear";
	Commands.Clear = true;
	Commands.ClearColor = {.25f, .5f, .75f, 1};
	Commands.TransitionFrom = EResourceState::Present;
	Commands.TransitionTo = EResourceState::RenderTarget;
	return Commands;
}

FPassCommands PresentCommands()
{
	FPassCommands Commands;
	Commands.Name = "present";
	Commands.TransitionFrom = EResourceState::RenderTarget;
	Commands.TransitionTo = EResourceState::Present;
	return Commands;
}
} // namespace

int main()
{
	try
	{
		FRHIBackendRegistry Registry;
		RegisterD3D12RHIBackend(Registry);
		FRHIDeviceDesc Desc;
		Desc.RequiredFeatures = {ERHIFeature::Graphics, ERHIFeature::Readback};
		Desc.OptionalFeatures = {ERHIFeature::RayTracing};
		auto Device = Registry.CreateDevice(ERHIBackend::D3D12, Desc);
		auto OtherDevice = Registry.CreateDevice(ERHIBackend::D3D12, Desc);
		HYP_CHECK(Device->GetCapabilities().Backend == ERHIBackend::D3D12);
		HYP_CHECK(Device->GetCapabilities().ShaderFormat == EShaderFormat::Dxil);
		HYP_CHECK(Device->QueryFeature(ERHIFeature::Graphics).Enabled);
		HYP_CHECK(!Device->QueryFeature(ERHIFeature::RayTracing).Enabled);
		Desc.RequiredFeatures = {ERHIFeature::RayTracing};
		Rejects(
		    [&]
		    {
			    Registry.CreateDevice(ERHIBackend::D3D12, Desc);
		    });
		Rejects(
		    [&]
		    {
			    Device->CreateSwapchain({});
		    });

		// Both devices and resources exist before any window is created.
		const std::array<std::uint32_t, 3> Indices{0, 1, 2};
		auto Own = Device->CreateBuffer(std::as_bytes(std::span(Indices)));
		auto Foreign = OtherDevice->CreateBuffer(std::as_bytes(std::span(Indices)));
		HYP_CHECK(Own.Payload->GetDeviceIdentity() != Foreign.Payload->GetDeviceIdentity());
		FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
		                         std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache");
		FPipelineDesc PipelineDesc;
		PipelineDesc.Vertex =
		    Compiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, Device->GetCapabilities().ShaderFormat);
		PipelineDesc.Pixel =
		    Compiler.Compile("Triangle.hlsl", "PSMain", EShaderStage::Pixel, Device->GetCapabilities().ShaderFormat);
		PipelineDesc.Attributes = {{"POSITION", 0, EVertexFormat::Float3, offsetof(FVertex, Position)},
		                           {"COLOR", 0, EVertexFormat::Float4, offsetof(FVertex, Color)},
		                           {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FVertex, Uv)}};
		auto Pipeline = Device->CreatePipeline(PipelineDesc);
		auto ForeignPipeline = OtherDevice->CreatePipeline(PipelineDesc);
		auto Texture = OtherDevice->CreateTexture({1, 1, EColorSpace::Linear, {1, 1, 1, 1}});

		FWindow Window("RHI ownership test", {64, 64}, true);
		FWindow OtherWindow("RHI second swapchain", {64, 64}, true);
		auto Swapchain = Device->CreateSwapchain({Window.Surface(), Window.PixelSize()});
		auto OtherSwapchain = Device->CreateSwapchain({OtherWindow.Surface(), OtherWindow.PixelSize()});
		Swapchain->BeginFrame(Window.PixelSize());
		OtherSwapchain->BeginFrame(OtherWindow.PixelSize());
		auto Commands = ClearCommands();
		FDrawPacket Draw;
		Draw.Pipeline = Pipeline;
		Draw.Vertices = Foreign;
		Draw.Indices = Own;
		Draw.VertexStride = 4;
		Draw.IndexCount = 3;
		Commands.Draws = {Draw};
		Rejects(
		    [&]
		    {
			    Swapchain->Record(0, Commands);
		    });
		Commands.Draws[0].Vertices = {std::make_shared<FForeignBuffer>()};
		Rejects(
		    [&]
		    {
			    Swapchain->Record(0, Commands);
		    });
		Commands.Draws[0].Vertices = Own;
		Commands.Draws[0].Pipeline = ForeignPipeline;
		Rejects(
		    [&]
		    {
			    Swapchain->Record(0, Commands);
		    });
		Commands.Draws[0].Pipeline = Pipeline;
		Commands.Draws[0].Texture = Texture;
		Rejects(
		    [&]
		    {
			    Swapchain->Record(0, Commands);
		    });

		auto Clear = Swapchain->Record(0, ClearCommands());
		auto Present = Swapchain->Record(1, PresentCommands());
		auto OtherClear = OtherSwapchain->Record(0, ClearCommands());
		auto OtherPresent = OtherSwapchain->Record(1, PresentCommands());
		Rejects(
		    [&]
		    {
			    Swapchain->Record(0, ClearCommands());
		    });
		const std::array<FRecordedList, 2> ForeignLists{OtherClear, Present};
		Rejects(
		    [&]
		    {
			    Swapchain->EndFrame(ForeignLists, false);
		    });
		const std::array<FRecordedList, 2> DuplicateLists{Clear, Clear};
		Rejects(
		    [&]
		    {
			    Swapchain->EndFrame(DuplicateLists, false);
		    });
		const std::array<FRecordedList, 2> Lists{Clear, Present};
		auto Image = Swapchain->EndFrame(Lists, false, true);
		HYP_CHECK(Image.Width == 64 && Image.Height == 64 && Image.Rgba[0] > .24f && Image.Rgba[0] < .26f);
		const std::array<FRecordedList, 2> OtherLists{OtherClear, OtherPresent};
		OtherSwapchain->EndFrame(OtherLists, false);
		OtherSwapchain.reset();
		OtherSwapchain = Device->CreateSwapchain({OtherWindow.Surface(), OtherWindow.PixelSize()});
		OtherSwapchain->BeginFrame(OtherWindow.PixelSize());
		Rejects(
		    [&]
		    {
			    OtherSwapchain->EndFrame(OtherLists, false);
		    });
		const std::array<FRecordedList, 2> RecreatedLists{OtherSwapchain->Record(0, ClearCommands()),
		                                                  OtherSwapchain->Record(1, PresentCommands())};
		OtherSwapchain->EndFrame(RecreatedLists, false);
		Swapchain->BeginFrame({80, 48});
		Rejects(
		    [&]
		    {
			    Swapchain->EndFrame(Lists, false);
		    });
		const std::array<FRecordedList, 2> ResizedLists{Swapchain->Record(0, ClearCommands()),
		                                                Swapchain->Record(1, PresentCommands())};
		Image = Swapchain->EndFrame(ResizedLists, false, true);
		HYP_CHECK(Image.Width == 80 && Image.Height == 48);
		Device->WaitIdle();
		OtherDevice->WaitIdle();
		HYP_CHECK(Device->Statistics().ValidationErrors == 0 && OtherDevice->Statistics().ValidationErrors == 0);

		// Swapchain/resources retain native state after the public device owner is released.
		Device.reset();
		Swapchain->BeginFrame({80, 48});
		const std::array<FRecordedList, 2> RetainedLists{Swapchain->Record(0, ClearCommands()),
		                                                 Swapchain->Record(1, PresentCommands())};
		Image = Swapchain->EndFrame(RetainedLists, false, true);
		HYP_CHECK(Image.Width == 80);
		Swapchain->WaitIdle();
		OtherSwapchain->WaitIdle();
		HYP_CHECK(OtherDevice->Statistics().ValidationErrors == 0);
		std::cout
		    << "Headless D3D12 creation, capabilities, resource ownership, swapchain isolation and lifetime passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
