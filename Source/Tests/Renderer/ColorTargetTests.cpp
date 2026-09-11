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
	throw std::runtime_error("Invalid MRT operation accepted");
}

struct FColorFixture
{
	IRHIDevice& Device;
	IRHISwapchain& Swapchain;
	std::array<FTexture, 2> Colors;
	FDrawPacket Producer;
	FDrawPacket Consumer;
	FPipelineDesc Description;

	FColorFixture(IRHIDevice& InDevice, IRHISwapchain& InSwapchain) : Device(InDevice), Swapchain(InSwapchain)
	{
		Colors[0] = Device.CreateColorTexture({32, 32, ERHIColorFormat::Rgba8Unorm, {.25f, .5f, .75f, 1}});
		Colors[1] = Device.CreateColorTexture({32, 32, ERHIColorFormat::Rgba16Float, {4, 2, 1, 1}});
		const auto Root = std::filesystem::absolute("color-target-test/source");
		std::filesystem::create_directories(Root);
		std::ofstream(Root / "Color.hlsl") << R"(
float4 VSMain(float2 InPosition:POSITION):SV_Position {return float4(InPosition,0,1);}
struct FOutput {float4 A:SV_Target0;float4 B:SV_Target1;};
FOutput PSWrite()
{
    FOutput O;O.A=float4(.25,.5,.75,1);O.B=float4(4,2,1,1);return O;
}
Texture2D<float4> A:register(t0);
Texture2D<float4> B:register(t1);
float4 PSRead():SV_Target0
{
    float4 Hdr=B.Load(int3(16,16,0));
    return float4(A.Load(int3(16,16,0)).r+Hdr.r/16,Hdr.g/4,Hdr.b*.75,1);
}
)";
		FShaderCompiler Compiler(Root, "color-target-test/cache");
		const std::array<FVec2, 3> Points{{{-1, -1}, {3, -1}, {-1, 3}}};
		const std::array<std::uint32_t, 3> Indices{0, 1, 2};
		Producer.Vertices = Device.CreateBuffer(std::as_bytes(std::span(Points)));
		Producer.Indices = Device.CreateBuffer(std::as_bytes(std::span(Indices)));
		Producer.VertexStride = sizeof(FVec2);
		Producer.IndexCount = 3;
		Producer.Scissor = {0, 0, 32, 32};
		Description.Vertex = Compiler.Compile("Color.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
		Description.Pixel = Compiler.Compile("Color.hlsl", "PSWrite", EShaderStage::Pixel, EShaderFormat::Dxil);
		Description.VertexStride = sizeof(FVec2);
		Description.Attributes = {{"POSITION", 0, EVertexFormat::Float2, 0}};
		Description.Layout = Device.CreateBindingLayout({});
		Description.Target.ColorCount = 2;
		Description.Target.ColorFormats[1] = ERHIColorFormat::Rgba16Float;
		Producer.Pipeline = Device.CreatePipeline(Description);
		Producer.Bindings = Device.CreateBindingSet({Description.Layout, {}});
		auto Invalid = Description;
		Invalid.Target.ColorCount = 1;
		Rejects(
		    [&]
		    {
			    Device.CreatePipeline(Invalid);
		    });
		Consumer = Producer;
		Consumer.Scissor = {0, 0, 384, 288};
		auto Read = Description;
		Read.Target = {};
		Read.Pixel = Compiler.Compile("Color.hlsl", "PSRead", EShaderStage::Pixel, EShaderFormat::Dxil);
		Read.Layout = Device.CreateBindingLayout({{{ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel, 0},
		                                           {ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel, 1}}});
		Consumer.Pipeline = Device.CreatePipeline(Read);
		Consumer.Bindings = Device.CreateBindingSet({Read.Layout, {{0, {Colors[0]}}, {1, {Colors[1]}}}});
	}

	FGraphTexture Import(FRenderGraph& InGraph, std::size_t InIndex, bool bInInitialized) const
	{
		FGraphTextureImport Resource{"Color " + std::to_string(InIndex),
		                             FRenderTarget::FromTexture(Colors[InIndex]),
		                             {32, 32},
		                             ERHIDepthFormat::None,
		                             EResourceState::ShaderRead,
		                             bInInitialized};
		Resource.ColorFormat = Colors[InIndex].Payload->GetInfo().ColorFormat;
		auto Result = InGraph.Import(Resource);
		InGraph.Export(Result, EResourceState::ShaderRead);
		return Result;
	}

	FRenderGraph Graph(bool bInWrite) const
	{
		FRenderGraph Result;
		const std::array Textures{Import(Result, 0, !bInWrite), Import(Result, 1, !bInWrite)};
		if (bInWrite)
		{
			FGraphicsPass Pass;
			Pass.Name = "MRT write";
			Pass.Viewport = FViewport{0, 0, 32, 32};
			for (const auto Texture : Textures)
			{
				Pass.Colors.push_back({Texture, {EAttachmentLoad::Clear}});
			}
			Pass.Batches.push_back({{{Producer}}});
			Result.Add(Pass);
		}
		auto Read = MakeColorPass(Result, "MRT read", EAttachmentLoad::Clear);
		Read.Reads = {Textures[0], Textures[1]};
		Read.Batches[0].Commands.Draws = {Consumer};
		Result.Add(Read);
		return Result;
	}

	FImage Render(std::vector<FPassCommands> InPlan)
	{
		Swapchain.BeginFrame({384, 288});
		try
		{
			std::vector<FRecordedList> Lists;
			for (std::size_t Index = 0; Index < InPlan.size(); ++Index)
			{
				Lists.push_back(Swapchain.Record(static_cast<unsigned>(Index), InPlan[Index]));
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

void CheckPixel(const FImage& InImage)
{
	const auto Offset = (144 * InImage.Width + 192) * 4;
	HYP_CHECK(std::abs(InImage.Rgba[Offset] - .5f) < .008f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 1] - .5f) < .008f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 2] - .75f) < .008f);
}

void CheckInvalidTargets(FColorFixture& InFixture)
{
	auto Plan = InFixture.Graph(true).Compile();
	auto Wrong = Plan;
	Wrong.front().Colors[1].Format = ERHIColorFormat::Rgba32Float;
	Rejects(
	    [&]
	    {
		    InFixture.Render(Wrong);
	    });
	Wrong = Plan;
	Wrong.front().Colors[1] = Wrong.front().Colors[0];
	Rejects(
	    [&]
	    {
		    InFixture.Render(Wrong);
	    });
	Wrong = Plan;
	Wrong.front().SampledTextures.push_back(InFixture.Colors[0]);
	Rejects(
	    [&]
	    {
		    InFixture.Render(Wrong);
	    });
	Wrong = Plan;
	Wrong.front().Colors.pop_back();
	Rejects(
	    [&]
	    {
		    InFixture.Render(Wrong);
	    });
	// Recording and cancellation must not commit the abandoned transitions.
	InFixture.Swapchain.BeginFrame({384, 288});
	auto Recorded = InFixture.Swapchain.Record(0, Plan.front());
	InFixture.Swapchain.CancelFrame();
	Recorded = {};
	CheckPixel(InFixture.Render(InFixture.Graph(true).Compile()));
}

void CheckSrgbMrt(FColorFixture& InFixture)
{
	const auto Color = InFixture.Device.CreateColorTexture({384, 288, ERHIColorFormat::Rgba8Unorm});
	FPassCommands Pass;
	Pass.Name = "MRT slot one sRGB";
	Pass.Colors = {{FRenderTarget::FromTexture(Color), {EAttachmentLoad::Clear}},
	               {FRenderTarget::Backbuffer(), {EAttachmentLoad::Clear}, {}, true}};
	Pass.Transitions = {{FRenderTarget::FromTexture(Color), EResourceState::ShaderRead, EResourceState::RenderTarget},
	                    {FRenderTarget::Backbuffer(), EResourceState::Present, EResourceState::RenderTarget}};
	HYP_CHECK(Pass.GetGraphicsTarget().GetColorFormat(1) == ERHIColorFormat::Rgba8Srgb);
	auto Description = InFixture.Description;
	Description.Target = Pass.GetGraphicsTarget();
	auto Draw = InFixture.Producer;
	Draw.Pipeline = InFixture.Device.CreatePipeline(Description);
	Draw.Scissor = {0, 0, 384, 288};
	Pass.SharedDraws = std::make_shared<const std::vector<FDrawPacket>>(std::vector{Draw});
	FPassCommands Restore;
	Restore.Transitions = {
	    {FRenderTarget::FromTexture(Color), EResourceState::RenderTarget, EResourceState::ShaderRead},
	    {FRenderTarget::Backbuffer(), EResourceState::RenderTarget, EResourceState::Present}};
	InFixture.Render({Pass, Restore});
	InFixture.Render({Pass, Restore}); // Exercise the retained native plan before changing its target.
	auto Wrong = Pass;
	Wrong.Colors[1].bSrgb = false;
	Rejects(
	    [&]
	    {
		    InFixture.Render({Wrong, Restore});
	    });
	Description.Target.ColorFormats[1] = ERHIColorFormat::Rgba8Unorm;
	Draw.Pipeline = InFixture.Device.CreatePipeline(Description);
	Wrong = Pass;
	Wrong.SharedDraws = std::make_shared<const std::vector<FDrawPacket>>(std::vector{Draw});
	Rejects(
	    [&]
	    {
		    InFixture.Render({Wrong, Restore});
	    });
	InFixture.Render({Pass, Restore});
	HYP_CHECK(InFixture.Device.Statistics().ValidationErrors == 0);
}

void CheckRetainedColor(FColorFixture& InFixture)
{
	auto Texture = InFixture.Device.CreateColorTexture({32, 32, ERHIColorFormat::Rgba16Float});
	InFixture.Device.WaitIdle(); // Retention check excludes initialization upload ownership.
	std::weak_ptr<IRHITexture> Weak = Texture.Payload;
	FPassCommands Pass;
	Pass.Name = "Retained color";
	Pass.Color = FColorAttachment{
	    FRenderTarget::FromTexture(Texture), {EAttachmentLoad::Clear}, {}, false, ERHIColorFormat::Rgba16Float};
	Pass.Transitions = {
	    {FRenderTarget::FromTexture(Texture), EResourceState::ShaderRead, EResourceState::RenderTarget}};
	InFixture.Swapchain.BeginFrame({384, 288});
	auto Recorded = InFixture.Swapchain.Record(0, Pass);
	Texture = {};
	Pass = {};
	HYP_CHECK(!Weak.expired());
	InFixture.Swapchain.CancelFrame();
	Recorded = {};
	InFixture.Device.CollectCompletedResources();
	HYP_CHECK(Weak.expired());
}
} // namespace

void RunColorTargetTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	FColorFixture Fixture(InDevice, InSwapchain);
	CheckPixel(Fixture.Render(Fixture.Graph(false).Compile()));
	CheckPixel(Fixture.Render(Fixture.Graph(true).Compile()));
	CheckInvalidTargets(Fixture);
	CheckRetainedColor(Fixture);
	CheckSrgbMrt(Fixture);
	Rejects(
	    [&]
	    {
		    InDevice.CreateColorTexture({0, 32, ERHIColorFormat::Rgba16Float});
	    });
	Rejects(
	    [&]
	    {
		    InDevice.CreateColorTexture({32, 32, ERHIColorFormat::Count});
	    });
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}
