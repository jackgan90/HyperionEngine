#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Support/TestSupport.h"
#include <iostream>

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
	Commands.bClear = true;
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

void CheckDeviceCapabilities(FRHIBackendRegistry& InRegistry, IRHIDevice& InDevice, FRHIDeviceDesc& InDesc)
{
	HYP_CHECK(InDevice.GetCapabilities().Backend == ERHIBackend::D3D12);
	HYP_CHECK(InDevice.GetCapabilities().ShaderFormat == EShaderFormat::Dxil);
	HYP_CHECK(InDevice.QueryFeature(ERHIFeature::Graphics).bEnabled);
	HYP_CHECK(!InDevice.QueryFeature(ERHIFeature::RayTracing).bEnabled);
	InDesc.RequiredFeatures = {ERHIFeature::RayTracing};
	Rejects(
	    [&]
	    {
		    InRegistry.CreateDevice(ERHIBackend::D3D12, InDesc);
	    });
	Rejects(
	    [&]
	    {
		    InDevice.CreateSwapchain({});
	    });
}

void CheckForeignDrawResources(IRHISwapchain& InSwapchain, FPassCommands& InCommands, const FBuffer& InOwn,
                               const FPipeline& InPipeline, const FPipeline& InForeignPipeline,
                               const FResourceBindingSet& InForeignBindings)
{
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, InCommands);
	    });
	InCommands.Draws[0].Vertices = {std::make_shared<FForeignBuffer>()};
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, InCommands);
	    });
	InCommands.Draws[0].Vertices = InOwn;
	InCommands.Draws[0].Pipeline = InForeignPipeline;
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, InCommands);
	    });
	InCommands.Draws[0].Pipeline = InPipeline;
	InCommands.Draws[0].Bindings = InForeignBindings;
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, InCommands);
	    });
}

FPipelineDesc TrianglePipeline(FShaderCompiler& InCompiler, IRHIDevice& InDevice)
{
	FPipelineDesc PipelineDesc;
	PipelineDesc.Vertex =
	    InCompiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, InDevice.GetCapabilities().ShaderFormat);
	PipelineDesc.Pixel =
	    InCompiler.Compile("Triangle.hlsl", "PSMain", EShaderStage::Pixel, InDevice.GetCapabilities().ShaderFormat);
	PipelineDesc.Attributes = {{"POSITION", 0, EVertexFormat::Float3, offsetof(FVertex, Position)},
	                           {"COLOR", 0, EVertexFormat::Float4, offsetof(FVertex, Color)},
	                           {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FVertex, Uv)}};
	PipelineDesc.VertexStride = sizeof(FVertex);
	PipelineDesc.Layout =
	    InDevice.CreateBindingLayout({{{ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Vertex, 0, 0, 1, 64},
	                                   {ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel, 0}}});
	return PipelineDesc;
}

void CheckRejectedFrameLists(IRHISwapchain& InSwapchain, const FRecordedList& InClear, const FRecordedList& InPresent,
                             const FRecordedList& InOtherClear)
{
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, ClearCommands());
	    });
	const std::array<FRecordedList, 2> ForeignLists{InOtherClear, InPresent};
	Rejects(
	    [&]
	    {
		    InSwapchain.EndFrame(ForeignLists, false);
	    });
	const std::array<FRecordedList, 2> DuplicateLists{InClear, InClear};
	Rejects(
	    [&]
	    {
		    InSwapchain.EndFrame(DuplicateLists, false);
	    });
}

void CheckSwapchainRecreation(IRHIDevice& InDevice, FWindow& InWindow, std::unique_ptr<IRHISwapchain>& InSwapchain,
                              std::span<const FRecordedList> InOldLists)
{
	InSwapchain.reset();
	InSwapchain = InDevice.CreateSwapchain({InWindow.Surface(), InWindow.PixelSize()});
	InSwapchain->BeginFrame(InWindow.PixelSize());
	Rejects(
	    [&]
	    {
		    InSwapchain->EndFrame(InOldLists, false);
	    });
	const std::array<FRecordedList, 2> Lists{InSwapchain->Record(0, ClearCommands()),
	                                         InSwapchain->Record(1, PresentCommands())};
	InSwapchain->EndFrame(Lists, false);
}

void CheckIndexShadowValidation(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, FDrawPacket InDraw)
{
	std::vector<std::uint32_t> Indices(99);
	for (std::size_t Index = 0; Index < Indices.size(); ++Index)
	{
		Indices[Index] = static_cast<std::uint32_t>(Index % 3);
	}
	Indices.back() = 3;
	const auto Bytes = std::as_bytes(std::span(Indices));
	InDraw.Indices = InDevice.CreateBuffer({Bytes.size(), BufferUsage(ERHIBufferUsage::Index)}, Bytes);
	FPassCommands Commands = ClearCommands();
	for (std::uint32_t Index = 0; Index < 33; ++Index)
	{
		InDraw.FirstIndex = Index * 3;
		Commands.Draws.push_back(InDraw);
	}
	// The invalid 33rd range remains checked after the bounded 32-entry range cache is full.
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Commands);
	    });
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Commands);
	    });
	const std::array<std::byte, 9> Partial{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},  std::byte{1},
	                                       std::byte{0}, std::byte{0}, std::byte{0}, std::byte{255}};
	InDraw.FirstIndex = 0;
	InDraw.Indices = InDevice.CreateBuffer({12, BufferUsage(ERHIBufferUsage::Index)}, Partial);
	Commands.Draws = {InDraw};
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Commands);
	    });
}

void CheckDeviceOwnership()
{
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	FRHIDeviceDesc Desc;
	Desc.RequiredFeatures = {ERHIFeature::Graphics, ERHIFeature::Readback};
	Desc.OptionalFeatures = {ERHIFeature::RayTracing};
	auto Device = Registry.CreateDevice(ERHIBackend::D3D12, Desc);
	auto OtherDevice = Registry.CreateDevice(ERHIBackend::D3D12, Desc);
	CheckDeviceCapabilities(Registry, *Device, Desc);

	// Both devices and resources exist before any window is created.
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	const std::array<FVertex, 3> Vertices{
	    {{{0, .5f, 0}, {1, 0, 0, 1}, {}}, {{-.5f, -.5f, 0}, {0, 1, 0, 1}, {}}, {{.5f, -.5f, 0}, {0, 0, 1, 1}, {}}}};
	auto Own = Device->CreateBuffer(std::as_bytes(std::span(Vertices)));
	auto OwnIndices = Device->CreateBuffer(std::as_bytes(std::span(Indices)));
	auto Foreign = OtherDevice->CreateBuffer(std::as_bytes(std::span(Vertices)));
	HYP_CHECK(Own.Payload->GetDeviceIdentity() != Foreign.Payload->GetDeviceIdentity());
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
	                         std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache");
	FPipelineDesc PipelineDesc = TrianglePipeline(Compiler, *Device);
	auto Pipeline = Device->CreatePipeline(PipelineDesc);
	const auto ForeignDescription = TrianglePipeline(Compiler, *OtherDevice);
	auto ForeignPipeline = OtherDevice->CreatePipeline(ForeignDescription);
	auto Texture = OtherDevice->CreateTexture({1, 1, EColorSpace::Linear, {1, 1, 1, 1}});
	Rejects(
	    [&]
	    {
		    Device->CreateBindingSet({PipelineDesc.Layout, {{1, {Texture}}}});
	    });
	const auto ForeignBindings = OtherDevice->CreateBindingSet({ForeignDescription.Layout, {{1, {Texture}}}});
	const auto OwnTexture = Device->CreateTexture({1, 1, EColorSpace::Linear, {1, 1, 1, 1}});
	const auto OwnBindings = Device->CreateBindingSet({PipelineDesc.Layout, {{1, {OwnTexture}}}});
	const auto Page = Device->CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
	const auto Transform = Identity();
	const auto Slice = Device->PublishConstantSlice(Page, 0, std::as_bytes(std::span(&Transform, 1)));

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
	Draw.Indices = OwnIndices;
	Draw.VertexStride = sizeof(FVertex);
	Draw.Bindings = OwnBindings;
	Draw.ConstantBindings = {{0, Slice}};
	Draw.IndexCount = 3;
	Commands.Draws = {Draw};
	CheckForeignDrawResources(*Swapchain, Commands, Own, Pipeline, ForeignPipeline, ForeignBindings);
	Draw.Vertices = Own;
	CheckIndexShadowValidation(*Device, *Swapchain, Draw);

	auto Clear = Swapchain->Record(0, ClearCommands());
	auto Present = Swapchain->Record(1, PresentCommands());
	auto OtherClear = OtherSwapchain->Record(0, ClearCommands());
	auto OtherPresent = OtherSwapchain->Record(1, PresentCommands());
	CheckRejectedFrameLists(*Swapchain, Clear, Present, OtherClear);
	const std::array<FRecordedList, 2> Lists{Clear, Present};
	auto Image = Swapchain->EndFrame(Lists, false, true);
	HYP_CHECK(Image.Width == 64 && Image.Height == 64 && Image.Rgba[0] > .24f && Image.Rgba[0] < .26f);
	const std::array<FRecordedList, 2> OtherLists{OtherClear, OtherPresent};
	OtherSwapchain->EndFrame(OtherLists, false);
	CheckSwapchainRecreation(*Device, OtherWindow, OtherSwapchain, OtherLists);
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
	std::cout << "Headless D3D12 creation, capabilities, resource ownership, swapchain isolation and lifetime passed\n";
}
} // namespace

int main()
{
	try
	{
		CheckDeviceOwnership();
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
