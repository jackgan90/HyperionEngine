#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "RHI/DrawStateFixture.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>

namespace Hyperion::D3D12Private
{
void WriteNativeDrawPlanMetrics(const FBuffer& InAnchor, const std::shared_ptr<const FPassCommands>& InPrepared,
                                std::ostream& InOutput);
} // namespace Hyperion::D3D12Private

namespace
{
using namespace Hyperion;

FDrawPacket PrepareTriangle(IRHIDevice& InDevice)
{
	FShaderCompiler Compiler(TestShaderRoot(), std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache");
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

std::shared_ptr<const FPassCommands> Measure(IRHISwapchain& InSwapchain, const FDrawPacket& InDraw,
                                             const FDrawPacket* InAlternate, std::size_t InCount, int InWarmup,
                                             int InSamples, std::ostream& InOutput, bool bInOwned)
{
	FPassCommands Commands;
	Commands.Color = FColorAttachment{FRenderTarget::Backbuffer()};
	Commands.Name = "Prepared triangles";
	Commands.Color->Actions.Load = EAttachmentLoad::Clear;
	Commands.Color->Clear = {.025f, .035f, .065f, 1};
	Commands.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::Present, EResourceState::RenderTarget}};
	Commands.Draws.assign(InCount, InDraw);
	if (InAlternate)
	{
		for (std::size_t Index = 1; Index < InCount; Index += 2)
		{
			Commands.Draws[Index] = *InAlternate;
		}
	}
	if (bInOwned)
	{
		Commands.ShareDraws();
	}
	const auto Owned = std::make_shared<const FPassCommands>(std::move(Commands));
	FPassCommands Present;
	Present.Name = "Present";
	Present.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::RenderTarget, EResourceState::Present}};
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
	return Owned;
}

void WriteString(std::ostream& InOutput, std::string_view InText)
{
	constexpr std::string_view Hex = "0123456789abcdef";
	InOutput << '"';
	for (const auto Character : InText)
	{
		const auto Value = static_cast<unsigned char>(Character);
		if (Character == '"' || Character == '\\')
		{
			InOutput << '\\' << Character;
		}
		else if (Value < 32)
		{
			InOutput << "\\u00" << Hex[Value >> 4] << Hex[Value & 15];
		}
		else
		{
			InOutput << Character;
		}
	}
	InOutput << '"';
}

void WriteMetadata(std::ostream& InOutput, const char* InCsv, const FDeviceStats& InStats, int InWarmup, int InSamples,
                   bool bInOwned, bool bInSwitching)
{
	InOutput << "{\"schema\":\"M08PlanMetricsV1\",\"source\":\"separate-cache-exact-prepared-packets\",\"csv\":";
	WriteString(InOutput, InCsv);
	InOutput << ",\"source_root\":";
	WriteString(InOutput, HYP_SOURCE_DIR);
	InOutput << ",\"backend\":\"D3D12\",\"adapter\":";
	WriteString(InOutput, InStats.Adapter);
	InOutput << ",\"debug_layer\":" << (InStats.bDebugLayer ? "true" : "false")
	         << ",\"resolution\":[1440,900],\"vsync\":false,\"warmup\":" << InWarmup << ",\"samples\":" << InSamples
	         << ",\"path\":\"" << (bInOwned ? "owned" : "ordinary") << "\",\"workload\":\""
	         << (bInSwitching ? "switching" : "homogeneous") << "\",\"groups\":[";
}

std::pair<bool, bool> ParseFlags(int InArgumentCount, char** InArguments)
{
	bool bOwned = false;
	bool bSwitching = false;
	for (int Index = 4; Index < InArgumentCount; ++Index)
	{
		const std::string_view Flag(InArguments[Index]);
		if (Flag == "--owned" && !bOwned)
		{
			bOwned = true;
		}
		else if (Flag == "--switching" && !bSwitching)
		{
			bSwitching = true;
		}
		else
		{
			throw std::invalid_argument("Unknown or duplicate submission benchmark flag");
		}
	}
	return {bOwned, bSwitching};
}
} // namespace

int main(int InArgumentCount, char** InArguments)
{
	try
	{
		if (InArgumentCount < 4 || InArgumentCount > 6)
		{
			throw std::invalid_argument(
			    "Usage: submission_benchmark Output.csv Warmup Samples [--owned] [--switching]");
		}
		const int Warmup = std::stoi(InArguments[2]);
		const int Samples = std::stoi(InArguments[3]);
		const auto [bOwned, bSwitching] = ParseFlags(InArgumentCount, InArguments);
		HYP_CHECK(Warmup > 0 && Samples > 0);
		std::ofstream Output(InArguments[1]);
		Output.exceptions(std::ios::failbit | std::ios::badbit);
		Output << "draws,sample,record_ms\n" << std::fixed << std::setprecision(6);
		std::ofstream Metrics(std::string(InArguments[1]) + ".plan.json");
		Metrics.exceptions(std::ios::failbit | std::ios::badbit);
		FRHIBackendRegistry Registry;
		RegisterD3D12RHIBackend(Registry);
		auto Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
		std::optional<Tests::FDrawStateFixture> Switching;
		if (bSwitching)
		{
			Switching.emplace(*Device, false);
		}
		const auto Draw = Switching ? Switching->A : PrepareTriangle(*Device);
		Device->WaitIdle();
		WriteMetadata(Metrics, InArguments[1], Device->Statistics(), Warmup, Samples, bOwned, bSwitching);
		FWindow Window("CPU submission benchmark", {1440, 900}, true);
		auto Swapchain = Device->CreateSwapchain({Window.Surface(), Window.PixelSize()});
		for (const auto Count : {0U, 1U, 100U, 300U, 600U, 1200U})
		{
			const auto Prepared =
			    Measure(*Swapchain, Draw, Switching ? &Switching->B : nullptr, Count, Warmup, Samples, Output, bOwned);
			if (Count != 0)
			{
				Metrics << ',';
			}
			D3D12Private::WriteNativeDrawPlanMetrics(Draw.Vertices, Prepared, Metrics);
		}
		Metrics << "]}\n";
		Metrics.close();
		Output.close();
		Device->WaitIdle();
		HYP_CHECK(Device->Statistics().ValidationErrors == 0);
		const auto Stats = Device->Statistics();
		std::cout << "Adapter: " << Stats.Adapter << "; debug layer: " << (Stats.bDebugLayer ? "true" : "false")
		          << "; workload: " << (bSwitching ? "switching" : "homogeneous")
		          << "; path: " << (bOwned ? "owned" : "ordinary") << '\n';
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
