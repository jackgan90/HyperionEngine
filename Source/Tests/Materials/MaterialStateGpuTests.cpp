#include "Hyperion/Renderer/MaterialPipeline.h"
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
	throw std::runtime_error("Invalid material graphics state was accepted");
}

struct FStateFixture
{
	IRHIDevice& Device;
	IRHISwapchain& Swapchain;
	FPipelineDesc Pipeline;
	FBuffer Vertices;
	FBuffer Indices;
	FResourceBindingSet Bindings;

	FStateFixture(IRHIDevice& InDevice, IRHISwapchain& InSwapchain) : Device(InDevice), Swapchain(InSwapchain)
	{
		const auto Root = std::filesystem::absolute("material-state-gpu/source");
		std::filesystem::create_directories(Root);
		std::ofstream(Root / "State.hlsl") << R"(
cbuffer StateData : register(b0) { float4 Color; float Depth; };
float4 VSMain(float2 InPosition : POSITION) : SV_Position { return float4(InPosition, Depth, 1); }
float4 PSMain() : SV_Target0 { return Color; }
)";
		FShaderCompiler Compiler(Root, "material-state-gpu/cache");
		Pipeline.Vertex = Compiler.Compile("State.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
		Pipeline.Pixel = Compiler.Compile("State.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
		Pipeline.Layout = Device.CreateBindingLayout(
		    {{{ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Graphics, 0, 0, 1, 32}}});
		Pipeline.Attributes = {{"POSITION", 0, EVertexFormat::Float2, 0}};
		Pipeline.VertexStride = sizeof(float) * 2;
		Pipeline.Target.Depth = ERHIDepthFormat::D32S8;
		Bindings = Device.CreateBindingSet({Pipeline.Layout, {}});
		const std::array<float, 6> Positions{-1, -1, 3, -1, -1, 3};
		const std::array<std::uint32_t, 3> IndexData{0, 1, 2};
		Vertices = Device.CreateBuffer(std::as_bytes(std::span(Positions)));
		Indices = Device.CreateBuffer(std::as_bytes(std::span(IndexData)));
	}

	FDrawPacket Draw(FGraphicsState InState, FVec4 InColor, float InDepth = .5F, bool bInPixelShader = true)
	{
		auto Desc = Pipeline;
		Desc.State = InState;
		if (!bInPixelShader)
		{
			Desc.Pixel = {};
		}
		FDrawPacket Result;
		Result.Pipeline = Device.CreatePipeline(Desc);
		Result.Vertices = Vertices;
		Result.Indices = Indices;
		Result.VertexStride = Pipeline.VertexStride;
		Result.IndexCount = 3;
		Result.Scissor = {0, 0, 64, 64};
		Result.Bindings = Bindings;
		const std::array<float, 8> Values{InColor.X, InColor.Y, InColor.Z, InColor.W, InDepth, 0, 0, 0};
		FBuffer Page = Device.CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
		Result.ConstantBindings = {{0, Device.PublishConstantSlice(Page, 0, std::as_bytes(std::span(Values)))}};
		return Result;
	}

	FImage Render(std::vector<FDrawPacket> InDraws, bool bInStencil = false)
	{
		Swapchain.BeginFrame({64, 64});
		FPassCommands Pass;
		Pass.Color = FColorAttachment{FRenderTarget::Backbuffer()};
		Pass.Name = "Graphics states";
		Pass.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::Present, EResourceState::RenderTarget}};
		Pass.Color->Actions.Load = EAttachmentLoad::Clear;
		Pass.Color->Clear = {0, 0, 0, 1};
		Pass.DepthStencil = FDepthStencilAttachment{FRenderTarget::FrameDepth(), ERHIDepthFormat::D32S8,
		                                            FAttachmentActions{EAttachmentLoad::Clear}};
		if (bInStencil)
		{
			Pass.DepthStencil->Stencil = FAttachmentActions{EAttachmentLoad::Clear};
		}
		Pass.Draws = std::move(InDraws);
		FPassCommands Present;
		Present.Name = "Present";
		Present.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::RenderTarget, EResourceState::Present}};
		try
		{
			const std::array Lists{Swapchain.Record(0, Pass), Swapchain.Record(1, Present)};
			return Swapchain.EndFrame(Lists, false, true);
		}
		catch (...)
		{
			Swapchain.CancelFrame();
			throw;
		}
	}
};

void Pixel(const FImage& InImage, FVec3 InExpected, std::uint32_t InX = 32, std::uint32_t InY = 32)
{
	const std::size_t Offset = (InY * InImage.Width + InX) * 4;
	HYP_CHECK(std::abs(InImage.Rgba[Offset] - InExpected.X) < .012F);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 1] - InExpected.Y) < .012F);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 2] - InExpected.Z) < .012F);
}

void CheckDepthAndBlend(FStateFixture& InFixture)
{
	FGraphicsState State;
	State.bDepthTest = State.bDepthWrite = true;
	State.DepthCompare = ERHICompare::Less;
	const auto Red = InFixture.Draw(State, {.8F, 0, 0, 1}, .2F);
	const auto Blue = InFixture.Draw(State, {0, 0, .8F, 1}, .7F);
	Pixel(InFixture.Render({Red, Blue}), {.8F, 0, 0});
	State.bDepthWrite = false;
	Pixel(InFixture.Render({InFixture.Draw(State, {.8F, 0, 0, 1}, .2F), Blue}), {0, 0, .8F});
	Pixel(InFixture.Render({Red, InFixture.Draw({}, {0, .8F, 0, 1})}), {0, .8F, 0});
	State = {};
	State.bBlend = true;
	State.SourceRgb = ERHIBlendFactor::SourceAlpha;
	State.DestinationRgb = ERHIBlendFactor::InverseSourceAlpha;
	Pixel(InFixture.Render({Blue, InFixture.Draw(State, {.8F, 0, 0, .5F})}), {.4F, 0, .4F});
	State.SourceRgb = ERHIBlendFactor::Constant;
	State.DestinationRgb = ERHIBlendFactor::InverseConstant;
	auto ConstantBlend = InFixture.Draw(State, {.8F, 0, 0, 1});
	ConstantBlend.DynamicState.BlendConstants = {.25F, .25F, .25F, 1};
	Pixel(InFixture.Render({Blue, ConstantBlend}), {.2F, 0, .6F});
	State = {};
	State.ColorWriteMask = 2;
	Pixel(InFixture.Render({Blue, InFixture.Draw(State, {.8F, .6F, .4F, 1})}), {0, .6F, .8F});
	State.ColorWriteMask = 15;
	State.SampleMask = 0;
	Pixel(InFixture.Render({Blue, InFixture.Draw(State, {.8F, 0, 0, 1})}), {0, 0, .8F});
}

void CheckStencilAndCulling(FStateFixture& InFixture)
{
	FGraphicsState State;
	State.bStencil = true;
	State.ColorWriteMask = 0;
	State.StencilWriteMask = 3;
	State.FrontStencil.Pass = State.BackStencil.Pass = ERHIStencilOp::Replace;
	auto Write = InFixture.Draw(State, {}, .5F, false);
	Write.DynamicState.StencilReference = 7;
	State.ColorWriteMask = 15;
	State.StencilWriteMask = 0;
	State.StencilReadMask = 3;
	State.FrontStencil.Compare = State.BackStencil.Compare = ERHICompare::Equal;
	auto Accepted = InFixture.Draw(State, {0, 0, .8F, 1});
	Accepted.DynamicState.StencilReference = 7;
	auto Rejected = InFixture.Draw(State, {.8F, 0, 0, 1});
	Rejected.DynamicState.StencilReference = 2;
	Pixel(InFixture.Render({Write, Accepted, Rejected}, true), {0, 0, .8F});
	FMaterialState MaterialState;
	MaterialState.Cull = EMaterialCull::Back;
	const FImage Normal = InFixture.Render({InFixture.Draw(ConvertMaterialState(MaterialState), {.8F, 0, 0, 1})});
	const FImage Mirrored =
	    InFixture.Render({InFixture.Draw(ConvertMaterialState(MaterialState, true), {.8F, 0, 0, 1})});
	const std::size_t Center = (32 * Normal.Width + 32) * 4;
	HYP_CHECK(std::abs(Normal.Rgba[Center] + Mirrored.Rgba[Center] - .8F) < .012F);
	HYP_CHECK(std::abs(Normal.Rgba[Center] - Mirrored.Rgba[Center]) > .7F);
	MaterialState.Cull = EMaterialCull::Front;
	const FImage Front = InFixture.Render({InFixture.Draw(ConvertMaterialState(MaterialState), {.8F, 0, 0, 1})});
	HYP_CHECK(std::abs(Front.Rgba[Center] - Mirrored.Rgba[Center]) < .012F);
}

void CheckStateReuse(FStateFixture& InFixture)
{
	auto Red = InFixture.Draw({}, {.8F, 0, 0, 1});
	auto Blue = InFixture.Draw({}, {0, 0, .8F, 1});
	Blue.Pipeline = Red.Pipeline;
	const std::array<float, 6> Positions{-1, -1, 3, -1, -1, 3};
	const std::array<std::uint32_t, 3> IndexData{0, 1, 2};
	Blue.Vertices = InFixture.Device.CreateBuffer(std::as_bytes(std::span(Positions)));
	Blue.Indices = InFixture.Device.CreateBuffer(std::as_bytes(std::span(IndexData)));
	Red.Scissor = {0, 0, 32, 64};
	Blue.Scissor = {32, 0, 64, 64};
	for (int Frame = 0; Frame < 4; ++Frame)
	{
		const auto Before = InFixture.Device.Statistics();
		const auto Image = InFixture.Render({Red, Red, Blue, Blue, Red});
		const auto After = InFixture.Device.Statistics();
		Pixel(Image, {.8F, 0, 0}, 16);
		Pixel(Image, {0, 0, .8F}, 48);
		HYP_CHECK(After.GraphicsPipelineBinds - Before.GraphicsPipelineBinds == 1);
		HYP_CHECK(After.GraphicsGeometryBinds - Before.GraphicsGeometryBinds == 7);
		HYP_CHECK(After.GraphicsDynamicBinds - Before.GraphicsDynamicBinds == 5);
	}
}

void CheckStateRejections(FStateFixture& InFixture)
{
	const auto Reject = [&](const std::function<void(FPipelineDesc&)>& InChange)
	{
		auto Desc = InFixture.Pipeline;
		InChange(Desc);
		Rejects(
		    [&]
		    {
			    InFixture.Device.CreatePipeline(Desc);
		    });
	};
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.State.bAlphaToCoverage = true;
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.Target.SampleCount = 4;
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.Target.ColorCount = 2;
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.State.bStencil = true;
		    InDesc.Target.Depth = ERHIDepthFormat::D32;
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.State.bDepthWrite = true;
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.Pixel = {};
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.Attributes.clear();
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.VertexStride = 4;
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.Attributes.front().Format = EVertexFormat::Int2;
	    });
	Reject(
	    [](FPipelineDesc& InDesc)
	    {
		    InDesc.State.bBlend = true;
		    InDesc.State.SourceAlpha = ERHIBlendFactor::SourceColor;
	    });
	auto Draw = InFixture.Draw({}, {1, 0, 0, 1});
	Draw.VertexOffset = 1;
	Rejects(
	    [&]
	    {
		    InFixture.Render({Draw});
	    });
	Draw.VertexOffset = 0;
	Draw.DynamicState.StencilReference = 256;
	Rejects(
	    [&]
	    {
		    InFixture.Render({Draw});
	    });
}

void CheckAttachmentDiscardGpu(IRHISwapchain& InSwapchain)
{
	FRenderGraph Graph;
	auto Pass = MakeColorPass(Graph, "Discard attachment planes", EAttachmentLoad::Discard);
	Pass.Color->Actions.Store = EAttachmentStore::Discard;
	Pass.DepthStencil =
	    FGraphDepthStencilAttachment{Graph.ImportFrameDepth(ERHIDepthFormat::D32S8),
	                                 FAttachmentActions{EAttachmentLoad::Discard, EAttachmentStore::Discard},
	                                 FAttachmentActions{EAttachmentLoad::Discard, EAttachmentStore::Discard}};
	Graph.Add(Pass);
	Pass.Name = "Initialize after discard";
	Pass.Color->Actions = {EAttachmentLoad::Clear};
	Pass.Color->Clear = {.25f, .5f, .75f, 1};
	Pass.DepthStencil->Depth = FAttachmentActions{EAttachmentLoad::Clear};
	Pass.DepthStencil->Stencil = FAttachmentActions{EAttachmentLoad::Clear};
	Graph.Add(Pass);
	const auto Commands = Graph.CompileAndConsume();
	InSwapchain.BeginFrame({64, 64});
	std::vector<FRecordedList> Lists;
	for (const auto& Command : Commands)
	{
		Lists.push_back(InSwapchain.Record(static_cast<std::uint32_t>(Lists.size()), Command));
	}
	const auto Image = InSwapchain.EndFrame(Lists, false, true);
	const auto Pixel = (32 * Image.Width + 32) * 4;
	HYP_CHECK(std::abs(Image.Rgba[Pixel] - .25f) < .01f);
	HYP_CHECK(std::abs(Image.Rgba[Pixel + 1] - .5f) < .01f);
}

void CheckGraphAttachmentInitialization()
{
	FRenderGraph Graph;
	auto Pass = MakeColorPass(Graph, "First view", EAttachmentLoad::Clear);
	Pass.DepthStencil = FGraphDepthStencilAttachment{Graph.ImportFrameDepth(ERHIDepthFormat::D32S8),
	                                                 FAttachmentActions{EAttachmentLoad::Clear}};
	Graph.Add(Pass);
	Pass.Name = "Undefined stencil";
	Pass.DepthStencil->Stencil = FAttachmentActions{};
	Graph.Add(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	Pass = MakeColorPass(Graph, "First view", EAttachmentLoad::Clear);
	Pass.DepthStencil =
	    FGraphDepthStencilAttachment{Graph.ImportFrameDepth(ERHIDepthFormat::D32S8),
	                                 FAttachmentActions{EAttachmentLoad::Clear, EAttachmentStore::Discard},
	                                 FAttachmentActions{EAttachmentLoad::Clear}};
	Graph.Add(Pass);
	Pass.Name = "Second view";
	Pass.DepthStencil->Depth->Load = EAttachmentLoad::Load;
	Graph.Add(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    }); // A new view cannot load explicitly discarded depth.
	Graph = {};
	Pass = MakeColorPass(Graph, "Initialized", EAttachmentLoad::Clear);
	Pass.DepthStencil = FGraphDepthStencilAttachment{Graph.ImportFrameDepth(ERHIDepthFormat::D32S8),
	                                                 FAttachmentActions{EAttachmentLoad::Clear},
	                                                 FAttachmentActions{EAttachmentLoad::Clear}};
	Graph.Add(Pass);
	HYP_CHECK(Graph.Compile().size() == 2);
	Graph = {};
	Pass = MakeColorPass(Graph, "Left", EAttachmentLoad::Clear);
	Pass.Viewport = FViewport{0, 0, 32, 64};
	Graph.Add(Pass);
	Pass.Name = "Uninitialized color region";
	Pass.Color->Actions.Load = EAttachmentLoad::Load;
	Pass.Viewport = FViewport{32, 0, 32, 64};
	Graph.Add(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
}
} // namespace

void RunMaterialStateGpuTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	FStateFixture Fixture(InDevice, InSwapchain);
	CheckAttachmentDiscardGpu(InSwapchain);
	CheckDepthAndBlend(Fixture);
	CheckStencilAndCulling(Fixture);
	CheckStateReuse(Fixture);
	CheckStateRejections(Fixture);
	CheckGraphAttachmentInitialization();
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}
