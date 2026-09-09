#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace
{
using namespace Hyperion;

FDrawPacket PrepareTriangle(IRHIDevice& InDevice)
{
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
	                         std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache");
	FPipelineDesc Desc;
	Desc.Vertex = Compiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
	Desc.Pixel = Compiler.Compile("Triangle.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
	Desc.Attributes = {{"POSITION", 0, EVertexFormat::Float3, offsetof(FVertex, Position)},
	                   {"COLOR", 0, EVertexFormat::Float4, offsetof(FVertex, Color)},
	                   {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FVertex, Uv)}};
	Desc.VertexStride = sizeof(FVertex);
	Desc.Layout =
	    InDevice.CreateBindingLayout({{{ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Vertex, 0, 0, 1, 64},
	                                   {ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel, 0}}});
	const std::array<FVertex, 3> Vertices{{{{0, .005f, 0}, {1, 0, 0, 1}, {}},
	                                       {{-.005f, -.005f, 0}, {0, 1, 0, 1}, {}},
	                                       {{.005f, -.005f, 0}, {0, 0, 1, 1}, {}}}};
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	const auto Texture = InDevice.CreateTexture({1, 1, EColorSpace::Linear, {1, 1, 1, 1}});
	const auto Page = InDevice.CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
	const auto Transform = Identity();
	FDrawPacket Draw;
	Draw.Pipeline = InDevice.CreatePipeline(Desc);
	Draw.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Vertices)));
	Draw.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Draw.VertexStride = sizeof(FVertex);
	Draw.IndexCount = 3;
	Draw.Scissor = {0, 0, 1440, 900};
	Draw.Bindings = InDevice.CreateBindingSet({Desc.Layout, {{1, {Texture}}}});
	Draw.ConstantBindings = {{0, InDevice.PublishConstantSlice(Page, 0, std::as_bytes(std::span(&Transform, 1)))}};
	return Draw;
}

void Measure(IRHISwapchain& InSwapchain, const FDrawPacket& InDraw, std::size_t InCount, int InWarmup, int InSamples,
             std::ostream& InOutput, bool bInOwned)
{
	FPassCommands Commands;
	Commands.Name = "Prepared triangles";
	Commands.bClear = true;
	Commands.ClearColor = {.025f, .035f, .065f, 1};
	Commands.TransitionFrom = EResourceState::Present;
	Commands.TransitionTo = EResourceState::RenderTarget;
	Commands.Draws.assign(InCount, InDraw);
	if (bInOwned)
	{
		Commands.ShareDraws();
	}
	const auto Owned = std::make_shared<const FPassCommands>(std::move(Commands));
	FPassCommands Present;
	Present.Name = "Present";
	Present.TransitionFrom = EResourceState::RenderTarget;
	Present.TransitionTo = EResourceState::Present;
	for (int Frame = 0; Frame < InWarmup + InSamples; ++Frame)
	{
		InSwapchain.BeginFrame({1440, 900});
		const auto Start = std::chrono::steady_clock::now();
		const auto Recorded = bInOwned ? InSwapchain.RecordOwned(0, Owned) : InSwapchain.Record(0, *Owned);
		const auto Finish = std::chrono::steady_clock::now();
		const std::array<FRecordedList, 2> Lists{Recorded, InSwapchain.Record(1, Present)};
		InSwapchain.EndFrame(Lists, false);
		if (Frame >= InWarmup)
		{
			InOutput << InCount << ',' << Frame - InWarmup << ','
			         << std::chrono::duration<double, std::milli>(Finish - Start).count() << '\n';
		}
	}
	InSwapchain.WaitIdle();
}
} // namespace

int main(int InArgumentCount, char** InArguments)
{
	try
	{
		if (InArgumentCount != 4 && InArgumentCount != 5)
		{
			throw std::invalid_argument("Usage: submission_benchmark Output.csv Warmup Samples [--owned]");
		}
		const int Warmup = std::stoi(InArguments[2]);
		const int Samples = std::stoi(InArguments[3]);
		const bool bOwned = InArgumentCount == 5 && std::string_view(InArguments[4]) == "--owned";
		HYP_CHECK(InArgumentCount == 4 || bOwned);
		HYP_CHECK(Warmup > 0 && Samples > 0);
		std::ofstream Output(InArguments[1]);
		Output.exceptions(std::ios::failbit | std::ios::badbit);
		Output << "draws,sample,record_ms\n" << std::fixed << std::setprecision(6);
		FRHIBackendRegistry Registry;
		RegisterD3D12RHIBackend(Registry);
		auto Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
		const auto Draw = PrepareTriangle(*Device);
		Device->WaitIdle();
		FWindow Window("CPU submission benchmark", {1440, 900}, true);
		auto Swapchain = Device->CreateSwapchain({Window.Surface(), Window.PixelSize()});
		for (const auto Count : {0U, 1U, 100U, 300U, 600U, 1200U})
		{
			Measure(*Swapchain, Draw, Count, Warmup, Samples, Output, bOwned);
		}
		Device->WaitIdle();
		HYP_CHECK(Device->Statistics().ValidationErrors == 0);
		const auto Stats = Device->Statistics();
		std::cout << "Native lists created: " << Stats.CommandListsCreated << "; resets: " << Stats.CommandListResets
		          << "; pipeline binds: " << Stats.GraphicsPipelineBinds
		          << "; geometry binds: " << Stats.GraphicsGeometryBinds
		          << "; dynamic binds: " << Stats.GraphicsDynamicBinds << '\n';
		std::cout << "Prepared-packet recording completed; validation errors: 0\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
