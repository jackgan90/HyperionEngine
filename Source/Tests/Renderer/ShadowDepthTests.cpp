#include "Hyperion/Renderer/RenderGraph.h"
#include "Support/GraphTestSupport.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <fstream>

using namespace Hyperion;

namespace
{
void Rejects(const std::function<void()>& InAction)
{
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		return;
	}
	throw std::runtime_error("Invalid shadow depth operation accepted");
}

struct FDepthFixture
{
	IRHIDevice& Device;
	IRHISwapchain& Swapchain;
	FTexture Depth;
	FDrawPacket Caster;
	FDrawPacket Masked;
	FDrawPacket Receiver;
	FDrawPacket Reversed;

	FDepthFixture(IRHIDevice& InDevice, IRHISwapchain& InSwapchain) : Device(InDevice), Swapchain(InSwapchain)
	{
		Depth = Device.CreateDepthTexture({32, 32, 1});
		const auto Root = std::filesystem::absolute("shadow-depth-gpu/source");
		std::filesystem::create_directories(Root);
		std::ofstream(Root / "Depth.hlsl") << R"(
float4 VSMain(float3 InPosition : POSITION) : SV_Position { return float4(InPosition, 1); }
void PSMask(float4 InPosition : SV_Position) { clip(16 - InPosition.x); }
Texture2D<float> DepthMap : register(t0);
SamplerComparisonState Compare : register(s0);
float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
    float2 Uv = InPosition.xy / 64;
    return float4(DepthMap.SampleCmpLevelZero(Compare, Uv, .5),
                  DepthMap.SampleCmpLevelZero(Compare, Uv, .1), 0, 1);
}
)";
		FShaderCompiler Compiler(Root, "shadow-depth-gpu/cache");
		const std::array<FVec3, 3> Points{{{-1, -1, .25f}, {3, -1, .25f}, {-1, 3, .25f}}};
		const std::array<std::uint32_t, 3> Indices{0, 1, 2};
		Caster.Vertices = Device.CreateBuffer(std::as_bytes(std::span(Points)));
		Caster.Indices = Device.CreateBuffer(std::as_bytes(std::span(Indices)));
		Caster.VertexStride = sizeof(FVec3);
		Caster.IndexCount = 3;
		Caster.Scissor = {0, 0, 32, 32};
		FPipelineDesc Pipeline;
		Pipeline.Vertex = Compiler.Compile("Depth.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
		Pipeline.VertexStride = Caster.VertexStride;
		Pipeline.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
		Pipeline.Layout = Device.CreateBindingLayout({});
		Pipeline.State.bDepthTest = Pipeline.State.bDepthWrite = true;
		Pipeline.State.ColorWriteMask = 0;
		Pipeline.Target = {false, ERHIDepthFormat::D32, 0};
		Caster.Pipeline = Device.CreatePipeline(Pipeline);
		Caster.Bindings = Device.CreateBindingSet({Pipeline.Layout, {}});
		Masked = Caster;
		Pipeline.Pixel = Compiler.Compile("Depth.hlsl", "PSMask", EShaderStage::Pixel, EShaderFormat::Dxil);
		Masked.Pipeline = Device.CreatePipeline(Pipeline);
		Receiver = Caster;
		Receiver.Scissor = {0, 0, 64, 64};
		Pipeline.State = {};
		Pipeline.Target = {};
		Pipeline.Pixel = Compiler.Compile("Depth.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
		FResourceBindingSlot Sampler{ERHIBindingKind::Sampler, ERHIShaderVisibility::Pixel};
		Sampler.bComparison = true;
		Pipeline.Layout =
		    Device.CreateBindingLayout({{{ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel}, Sampler}});
		Receiver.Pipeline = Device.CreatePipeline(Pipeline);
		FSamplerDesc Compare;
		Compare.bComparison = true;
		Compare.U = Compare.V = ERHIAddressMode::Clamp;
		Receiver.Bindings =
		    Device.CreateBindingSet({Pipeline.Layout, {{0, {Depth}}, {1, {Device.CreateSampler(Compare)}}}});
		Reversed = Receiver;
		Compare.Compare = ERHICompare::Greater;
		Reversed.Bindings =
		    Device.CreateBindingSet({Pipeline.Layout, {{0, {Depth}}, {1, {Device.CreateSampler(Compare)}}}});
		Rejects(
		    [&]
		    {
			    Device.CreateBindingSet({Pipeline.Layout, {{0, {Depth}}, {1, {Device.CreateSampler({})}}}});
		    });
		Pipeline.Target.ColorCount = 0;
		Pipeline.State.bDepthTest = Pipeline.State.bDepthWrite = true;
		Pipeline.State.ColorWriteMask = 0;
		Pipeline.Target.Depth = ERHIDepthFormat::D32;
		Rejects(
		    [&]
		    {
			    Device.CreatePipeline(Pipeline);
		    }); // Color output in a depth-only pipeline.
	}

	FGraphTexture Import(FRenderGraph& InGraph, bool bInInitialized = false, FTexture InTexture = {}) const
	{
		const auto Texture = InGraph.Import({"Shadow texture",
		                                     FRenderTarget::FromTexture(InTexture ? InTexture : Depth),
		                                     {32, 32},
		                                     ERHIDepthFormat::D32,
		                                     EResourceState::ShaderRead,
		                                     bInInitialized});
		InGraph.Export(Texture, EResourceState::ShaderRead);
		return Texture;
	}

	FGraphicsPass Producer(FRenderGraph& InGraph) const
	{
		FGraphicsPass Pass;
		Pass.Name = "Shadow write";
		Pass.DepthStencil = FGraphDepthStencilAttachment{Import(InGraph), FAttachmentActions{EAttachmentLoad::Clear}};
		Pass.Batches.push_back({});
		return Pass;
	}

	FGraphicsPass Consumer(FRenderGraph& InGraph, bool bInReverse = false, bool bInInitialized = false) const
	{
		auto Pass = MakeColorPass(InGraph, "Shadow compare", EAttachmentLoad::Clear);
		Pass.Reads = {Import(InGraph, bInInitialized)};
		Pass.Batches[0].Commands.Draws = {bInReverse ? Reversed : Receiver};
		return Pass;
	}

	FImage Render(const FRenderGraph& InGraph)
	{
		const auto Plan = InGraph.Compile();
		Swapchain.BeginFrame({64, 64});
		try
		{
			std::vector<FRecordedList> Lists;
			for (std::size_t Index = 0; Index < Plan.size(); ++Index)
			{
				Lists.push_back(Swapchain.Record(static_cast<std::uint32_t>(Index), Plan[Index]));
			}
			return Swapchain.EndFrame(Lists, false, true);
		}
		catch (...)
		{
			Swapchain.CancelFrame();
			throw;
		}
	}
};

void Pixel(const FImage& InImage, unsigned InX, float InRed, float InGreen)
{
	const auto Offset = (32 * InImage.Width + InX) * 4;
	HYP_CHECK(std::abs(InImage.Rgba[Offset] - InRed) < .02f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 1] - InGreen) < .02f);
}

void CheckPixels(FDepthFixture& InFixture)
{
	FRenderGraph Graph;
	Graph.Add(InFixture.Consumer(Graph, false, true));
	Pixel(InFixture.Render(Graph), 32, 1, 1); // Queued initial clear is visible without a CPU idle.
	Graph = {};
	auto Write = InFixture.Producer(Graph);
	Write.Batches[0].Commands.Draws = {InFixture.Caster};
	Graph.Add(Write);
	Graph.Add(InFixture.Consumer(Graph));
	const auto Plan = Graph.Compile();
	HYP_CHECK(!Plan[0].HasColor() && Plan[0].Transitions[0].After == EResourceState::DepthWrite);
	HYP_CHECK(Plan[1].Transitions.back().After == EResourceState::ShaderRead);
	Pixel(InFixture.Render(Graph), 32, 0, 1);
	Graph = {};
	Graph.Add(InFixture.Consumer(Graph, true, true));
	Pixel(InFixture.Render(Graph), 32, 1, 0);
	Graph = {};
	Write = InFixture.Producer(Graph);
	Write.Batches[0].Commands.Draws = {InFixture.Masked};
	Graph.Add(Write);
	Graph.Add(InFixture.Consumer(Graph));
	const auto Masked = InFixture.Render(Graph);
	Pixel(Masked, 16, 0, 1);
	Pixel(Masked, 48, 1, 1);
	Graph = {};
	Graph.Add(InFixture.Producer(Graph)); // Removal/empty cascade must erase the previous occluder.
	Graph.Add(InFixture.Consumer(Graph));
	Pixel(InFixture.Render(Graph), 16, 1, 1);
}

void CheckGraphHazards(FDepthFixture& InFixture)
{
	FRenderGraph Graph;
	Graph.Add(InFixture.Consumer(Graph));
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	auto Write = InFixture.Producer(Graph);
	Write.Reads = {Write.DepthStencil->Texture};
	Graph.Add(Write);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	Write = InFixture.Producer(Graph);
	Write.DepthStencil->Depth->Load = EAttachmentLoad::Load;
	Graph.Add(Write);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	Write = InFixture.Producer(Graph);
	Write.Viewport = FViewport{0, 0, 16, 16};
	Graph.Add(Write);
	Graph.Add(InFixture.Consumer(Graph));
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
}

void CheckPhysicalDimensions(FDepthFixture& InFixture)
{
	const auto Info = InFixture.Depth.Payload->GetInfo();
	HYP_CHECK(Info.Width == 32 && Info.Height == 32 && Info.DepthFormat == ERHIDepthFormat::D32);
	for (const bool bDeferred : {false, true})
	{
		FRenderGraph Graph;
		FGraphTextureImport Import{"wrong physical size",
		                           FRenderTarget::FromTexture(InFixture.Depth),
		                           {16, 32},
		                           ERHIDepthFormat::D32,
		                           EResourceState::ShaderRead};
		if (bDeferred)
		{
			Import.Target.Texture = {};
			Import.Identity = InFixture.Depth.Payload;
			Import.Resolve = [&InFixture]
			{
				return InFixture.Depth;
			};
		}
		FGraphicsPass Clear;
		Clear.Name = "clear wrong extent";
		Clear.DepthStencil =
		    FGraphDepthStencilAttachment{Graph.Import(Import), FAttachmentActions{EAttachmentLoad::Clear}};
		Graph.Add(Clear);
		Rejects(
		    [&]
		    {
			    Graph.Compile();
		    });
	}
}

void CheckCancelledRetention(FDepthFixture& InFixture)
{
	FRenderGraph Graph;
	Graph.Add(InFixture.Producer(Graph));
	auto Plan = Graph.Compile();
	InFixture.Swapchain.BeginFrame({64, 64});
	const auto Recorded = InFixture.Swapchain.Record(0, Plan.front());
	InFixture.Swapchain.CancelFrame();
	InFixture.Swapchain.CancelFrame();
	HYP_CHECK(Recorded.Payload);
	// Unsubmitted transitions must not poison the next graph's ShaderRead entry state.
	Graph.Add(InFixture.Consumer(Graph));
	Pixel(InFixture.Render(Graph), 32, 1, 1);
	Graph = {};
	auto Texture = InFixture.Device.CreateDepthTexture({32, 32, 1});
	std::weak_ptr<IRHITexture> Weak = Texture.Payload;
	FGraphicsPass Clear;
	Clear.Name = "Retained clear";
	Clear.DepthStencil = FGraphDepthStencilAttachment{InFixture.Import(Graph, false, Texture),
	                                                  FAttachmentActions{EAttachmentLoad::Clear}};
	Texture = {};
	Graph.Add(Clear);
	Plan = Graph.Compile();
	InFixture.Swapchain.BeginFrame({64, 64});
	auto Retained = InFixture.Swapchain.Record(0, Plan.front());
	Plan.clear();
	Graph = {};
	Clear = {};
	HYP_CHECK(!Weak.expired()); // Even clear-only work retains its attachment.
	InFixture.Swapchain.CancelFrame();
	Retained = {};
	HYP_CHECK(Weak.expired());
}
} // namespace

void RunShadowDepthTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	HYP_CHECK(InDevice.GetCapabilities().bSampledDepthTargets);
	FDepthFixture Fixture(InDevice, InSwapchain);
	CheckPixels(Fixture);
	CheckGraphHazards(Fixture);
	CheckPhysicalDimensions(Fixture);
	CheckCancelledRetention(Fixture);
	Rejects(
	    [&]
	    {
		    InDevice.CreateDepthTexture({0, 32, 1});
	    });
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}
