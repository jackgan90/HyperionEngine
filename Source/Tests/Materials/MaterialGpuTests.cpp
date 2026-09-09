#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <fstream>
#include <iostream>

using namespace Hyperion;

void RunMaterialStateGpuTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
void RunShadowDepthTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
void RunMaterialPackingGpuTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
void RunMaterialCacheTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
void RunMaterialGpuCacheTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
void RunMaterialResourceTests(IRHIDevice& InDevice);
void RunMaterialSessionTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
void RunMaterialFrequencyTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);

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
	throw std::runtime_error("Invalid graphics binding was accepted");
}

struct FFixture
{
	FRHIBackendRegistry Registry;
	std::unique_ptr<IRHIDevice> Device;
	FWindow Window{"Material binding pixels", {64, 64}, true};
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FShaderCompiler> Compiler;

	FFixture()
	{
		RegisterD3D12RHIBackend(Registry);
		Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
		Swapchain = Device->CreateSwapchain({Window.Surface(), Window.PixelSize(), ERHIDepthFormat::D32S8});
		const auto Root = std::filesystem::absolute("material-gpu-test/source");
		std::filesystem::create_directories(Root);
		std::ofstream Shader(Root / "Binding.hlsl");
		Shader << R"(
cbuffer ObjectData : register(b0, space1) { float4 Offset; };
cbuffer SurfaceData : register(b1, space2) { float4 Gain; };
#if MAP_COUNT > 0
Texture2D Maps[MAP_COUNT] : register(t3, space2);
SamplerState SurfaceSampler : register(s3, space2);
Texture2D VertexMap : register(t0);
SamplerState VertexSampler : register(s0);
#endif
StructuredBuffer<float4> Data : register(t16, space2);
ByteAddressBuffer Raw : register(t17, space2);
struct FOutput { float4 Position : SV_Position; float Blue : COLOR0; };
FOutput VSMain(float3 InPosition : POSITION)
{
    FOutput Result;
    Result.Position = float4(InPosition + Offset.xyz, 1);
#if MAP_COUNT > 0
    Result.Blue = VertexMap.SampleLevel(VertexSampler, float2(.5, .5), 0).b;
#else
    Result.Blue = .3;
#endif
    return Result;
}
float4 PSMain(FOutput InInput) : SV_Target0
{
    float Red = 0;
#if MAP_COUNT > 0
    [unroll] for (int Index = 0; Index < MAP_COUNT; ++Index)
    {
        Red += Maps[Index].SampleLevel(SurfaceSampler, float2(.5, .5), 0).r;
    }
#endif
    return float4(Red + Data[0].x, asfloat(Raw.Load(0)), InInput.Blue, 1) * Gain;
}
)";
		Shader.close();
		Compiler = std::make_unique<FShaderCompiler>(Root, "material-gpu-test/cache");
	}
};

struct FDrawSetup
{
	FDrawPacket Draw;
	FResourceBindingLayoutDesc Layout;
	FResourceBindingSetDesc Set;
	float ExpectedRed = .1F;

	std::uint32_t AddSlot(FResourceBindingSlot InSlot)
	{
		const std::uint32_t Index = static_cast<std::uint32_t>(Layout.Slots.size());
		Layout.Slots.push_back(InSlot);
		return Index;
	}

	void AddResource(FResourceBindingSlot InSlot, std::vector<FResourceBindingValue> InValues)
	{
		Set.Entries.push_back({AddSlot(InSlot), std::move(InValues)});
	}
};

void AddGeometryAndConstants(IRHIDevice& InDevice, FDrawSetup& InSetup)
{
	const std::array<FVec3, 3> Vertices{{{-1, -1, .5F}, {3, -1, .5F}, {-1, 3, .5F}}};
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	InSetup.Draw.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Vertices)));
	InSetup.Draw.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	InSetup.Draw.VertexStride = sizeof(FVec3);
	InSetup.Draw.IndexCount = 3;
	InSetup.Draw.Scissor = {0, 0, 64, 64};
	FBuffer Page = InDevice.CreateBuffer({512, BufferUsage(ERHIBufferUsage::Constant)});
	const std::array<float, 4> Offset{0, 0, 0, 0};
	const std::array<float, 4> Gain{1, 1, 1, 1};
	const FBufferSlice Object = InDevice.PublishConstantSlice(Page, 0, std::as_bytes(std::span(Offset)));
	const FBufferSlice Surface = InDevice.PublishConstantSlice(Page, 256, std::as_bytes(std::span(Gain)));
	InSetup.Draw.ConstantBindings = {
	    {InSetup.AddSlot({ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Vertex, 0, 1, 1, 16}), Object},
	    {InSetup.AddSlot({ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Pixel, 1, 2, 1, 16}), Surface}};
	Rejects(
	    [&]
	    {
		    InDevice.PublishConstantSlice(Page, 0, std::as_bytes(std::span(Gain)));
	    });
	Rejects(
	    [&]
	    {
		    InDevice.PublishConstantSlice(Page, 512, std::as_bytes(std::span(Gain)));
	    });
	Rejects(
	    [&]
	    {
		    InDevice.ResetConstantBuffer(Page);
	    });
}

void AddReadBuffers(IRHIDevice& InDevice, FDrawSetup& InSetup)
{
	const std::array<float, 4> Data{.1F, 0, 0, 0};
	const std::array<float, 1> Raw{.2F};
	const FBuffer Structured = InDevice.CreateBuffer({sizeof(Data), BufferUsage(ERHIBufferUsage::StructuredRead)},
	                                                 std::as_bytes(std::span(Data)));
	const FBuffer RawBuffer =
	    InDevice.CreateBuffer({sizeof(Raw), BufferUsage(ERHIBufferUsage::RawRead)}, std::as_bytes(std::span(Raw)));
	InSetup.AddResource({ERHIBindingKind::StructuredBuffer, ERHIShaderVisibility::Pixel, 16, 2, 1, 0, 16},
	                    {FReadBufferView{Structured, ERHIBufferViewKind::Structured, 0, sizeof(Data), sizeof(Data)}});
	InSetup.AddResource({ERHIBindingKind::RawBuffer, ERHIShaderVisibility::Pixel, 17, 2},
	                    {FReadBufferView{RawBuffer, ERHIBufferViewKind::Raw, 0, sizeof(Raw), 0}});
}

void AddTextures(IRHIDevice& InDevice, FDrawSetup& InSetup, std::uint32_t InCount)
{
	if (InCount == 0)
	{
		return;
	}
	std::vector<FResourceBindingValue> Textures;
	for (std::uint32_t Index = 0; Index < InCount; ++Index)
	{
		const float Red = static_cast<float>(Index + 1) / 64;
		Textures.emplace_back(InDevice.CreateTexture({1, 1, EColorSpace::Linear, {Red, 0, 0, 1}}));
		InSetup.ExpectedRed += std::round(Red * 255) / 255;
	}
	InSetup.AddResource({ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel, 3, 2, InCount}, std::move(Textures));
	const FSampler Sampler = InDevice.CreateSampler({});
	InSetup.AddResource({ERHIBindingKind::Sampler, ERHIShaderVisibility::Pixel, 3, 2}, {Sampler});
	InSetup.AddResource({ERHIBindingKind::Texture2D, ERHIShaderVisibility::Vertex, 0, 0},
	                    {InDevice.CreateTexture({1, 1, EColorSpace::Linear, {0, 0, .3F, 1}})});
	InSetup.AddResource({ERHIBindingKind::Sampler, ERHIShaderVisibility::Vertex, 0, 0}, {Sampler});
}

FImage Render(FFixture& InFixture, const FDrawPacket& InDraw)
{
	InFixture.Swapchain->BeginFrame({64, 64});
	FPassCommands Draw;
	Draw.Name = "Material bindings";
	Draw.TransitionFrom = EResourceState::Present;
	Draw.TransitionTo = EResourceState::RenderTarget;
	Draw.bClear = true;
	Draw.ClearColor = {0, 0, 0, 1};
	Draw.Draws = {InDraw, InDraw};
	FPassCommands Invalid = Draw;
	++Invalid.Draws.front().ConstantBindings.front().Slice.Offset;
	Rejects(
	    [&]
	    {
		    InFixture.Swapchain->Record(0, Invalid);
	    });
	Invalid = Draw;
	++Invalid.Draws.front().ConstantBindings.front().Slice.Publication;
	Rejects(
	    [&]
	    {
		    InFixture.Swapchain->Record(0, Invalid);
	    });
	FPassCommands Present;
	Present.Name = "Present";
	Present.TransitionFrom = EResourceState::RenderTarget;
	Present.TransitionTo = EResourceState::Present;
	const auto Before = InFixture.Device->Statistics();
	const std::array<FRecordedList, 2> Lists{InFixture.Swapchain->Record(0, Draw),
	                                         InFixture.Swapchain->Record(1, Present)};
	const auto After = InFixture.Device->Statistics();
	HYP_CHECK(After.GraphicsRootBinds == Before.GraphicsRootBinds + 1);
	HYP_CHECK(After.GraphicsHeapBinds == Before.GraphicsHeapBinds + 1);
	HYP_CHECK(After.GraphicsConstantBinds == Before.GraphicsConstantBinds + InDraw.ConstantBindings.size());
	return InFixture.Swapchain->EndFrame(Lists, false, true);
}

void CheckRootSwitch(FFixture& InFixture, const FDrawSetup& InSetup, FPipelineDesc InPipeline)
{
	const auto Before = InFixture.Device->Statistics();
	FDrawPacket Other = InSetup.Draw;
	auto OtherLayout = InSetup.Layout;
	Other.ConstantBindings.push_back(
	    {static_cast<std::uint32_t>(OtherLayout.Slots.size()), InSetup.Draw.ConstantBindings.front().Slice});
	OtherLayout.Slots.push_back({ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Pixel, 3, 0, 1, 16});
	InPipeline.Layout = InFixture.Device->CreateBindingLayout(OtherLayout);
	Other.Pipeline = InFixture.Device->CreatePipeline(InPipeline);
	auto OtherSet = InSetup.Set;
	OtherSet.Layout = InPipeline.Layout;
	Other.Bindings = InFixture.Device->CreateBindingSet(OtherSet);
	InFixture.Swapchain->BeginFrame({64, 64});
	FPassCommands Pass;
	Pass.Name = "Root switch and repeated arguments";
	Pass.TransitionFrom = EResourceState::Present;
	Pass.TransitionTo = EResourceState::RenderTarget;
	Pass.bClear = true;
	Pass.Draws = {InSetup.Draw, Other, InSetup.Draw, InSetup.Draw};
	FPassCommands Present;
	Present.TransitionFrom = EResourceState::RenderTarget;
	Present.TransitionTo = EResourceState::Present;
	const std::array Lists{InFixture.Swapchain->Record(0, Pass), InFixture.Swapchain->Record(1, Present)};
	const auto Image = InFixture.Swapchain->EndFrame(Lists, false, true);
	const auto After = InFixture.Device->Statistics();
	HYP_CHECK(After.GraphicsRootBinds == Before.GraphicsRootBinds + 3);
	HYP_CHECK(After.GraphicsHeapBinds == Before.GraphicsHeapBinds + 1);
	HYP_CHECK(After.GraphicsConstantBinds ==
	          Before.GraphicsConstantBinds + 3 * InSetup.Draw.ConstantBindings.size() + 1);
	HYP_CHECK(std::abs(Image.Rgba[(32 * 64 + 32) * 4] - InSetup.ExpectedRed) < .01f);
	HYP_CHECK(After.ValidationErrors == 0);
}

void CheckPixels(FFixture& InFixture, std::uint32_t InTextureCount)
{
	FDrawSetup Setup;
	AddGeometryAndConstants(*InFixture.Device, Setup);
	AddReadBuffers(*InFixture.Device, Setup);
	AddTextures(*InFixture.Device, Setup, InTextureCount);
	Setup.Set.Layout = InFixture.Device->CreateBindingLayout(Setup.Layout);
	Setup.Draw.Bindings = InFixture.Device->CreateBindingSet(Setup.Set);
	FPipelineDesc Pipeline;
	Pipeline.Layout = Setup.Set.Layout;
	Pipeline.VertexStride = sizeof(float) * 3;
	const FShaderCompileOptions Options{{{"MAP_COUNT", std::to_string(InTextureCount)}}};
	Pipeline.Vertex =
	    InFixture.Compiler->Compile("Binding.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil, Options);
	Pipeline.Pixel =
	    InFixture.Compiler->Compile("Binding.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, Options);
	Pipeline.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
	auto WrongSet = Setup.Set;
	for (auto& Entry : WrongSet.Entries)
	{
		for (auto& Value : Entry.Values)
		{
			if (auto* View = std::get_if<FReadBufferView>(&Value); View && View->Kind == ERHIBufferViewKind::Structured)
			{
				View->Stride = 4; // A valid descriptor stride, incompatible with StructuredBuffer<float4>.
			}
		}
	}
	Rejects(
	    [&]
	    {
		    InFixture.Device->CreateBindingSet(WrongSet);
	    });
	auto WrongLayout = Setup.Layout;
	for (auto& Slot : WrongLayout.Slots)
	{
		if (Slot.Kind == ERHIBindingKind::StructuredBuffer)
		{
			Slot.StructureByteStride = 4;
		}
	}
	Pipeline.Layout = InFixture.Device->CreateBindingLayout(WrongLayout);
	Rejects(
	    [&]
	    {
		    InFixture.Device->CreatePipeline(Pipeline);
	    });
	Pipeline.Layout = Setup.Set.Layout;
	Setup.Draw.Pipeline = InFixture.Device->CreatePipeline(Pipeline);
	const FImage Image = Render(InFixture, Setup.Draw);
	CheckRootSwitch(InFixture, Setup, Pipeline);
	const std::size_t Pixel = (32 * Image.Width + 32) * 4;
	HYP_CHECK(std::abs(Image.Rgba[Pixel] - Setup.ExpectedRed) < .01F);
	HYP_CHECK(std::abs(Image.Rgba[Pixel + 1] - .2F) < .01F);
	HYP_CHECK(std::abs(Image.Rgba[Pixel + 2] - .3F) < .01F);
	HYP_CHECK(InFixture.Device->Statistics().ValidationErrors == 0);
}

void CheckIntegerTextureRejection(FFixture& InFixture)
{
	std::ofstream("material-gpu-test/source/Integer.hlsl") << R"(
Texture2D<COMPONENT_TYPE> Image : register(t0);
float4 VSMain(float2 InPosition : POSITION) : SV_Position { return float4(InPosition,0,1); }
float4 PSMain() : SV_Target0 { return float4(Image.Load(int3(0,0,0))); }
)";
	FPipelineDesc Pipeline;
	Pipeline.VertexStride = 8;
	Pipeline.Attributes = {{"POSITION", 0, EVertexFormat::Float2, 0}};
	Pipeline.Layout =
	    InFixture.Device->CreateBindingLayout({{{ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel}}});
	for (const std::string Type : {"int4", "uint4"})
	{
		const FShaderCompileOptions Options{{{"COMPONENT_TYPE", Type}}};
		Pipeline.Vertex =
		    InFixture.Compiler->Compile("Integer.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil, Options);
		Pipeline.Pixel =
		    InFixture.Compiler->Compile("Integer.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, Options);
		Rejects(
		    [&]
		    {
			    InFixture.Device->CreatePipeline(Pipeline);
		    });
	}
	HYP_CHECK(InFixture.Device->Statistics().ValidationErrors == 0);
}

void CheckAllocationRollback(FFixture& InFixture)
{
	FRHIDeviceDesc Desc;
	Desc.SamplerDescriptorCapacity = 1;
	Desc.ResourceDescriptorCapacity = 4;
	auto Device = InFixture.Registry.CreateDevice(ERHIBackend::D3D12, Desc);
	const FSampler Sampler = Device->CreateSampler({});
	FResourceBindingLayoutDesc LayoutDesc{{{ERHIBindingKind::Sampler, ERHIShaderVisibility::Vertex},
	                                       {ERHIBindingKind::Sampler, ERHIShaderVisibility::Pixel}}};
	const FResourceBindingLayout Layout = Device->CreateBindingLayout(LayoutDesc);
	Rejects(
	    [&]
	    {
		    Device->CreateBindingSet({Layout, {{0, {Sampler}}, {1, {Sampler}}}});
	    });
	LayoutDesc.Slots.resize(1);
	const FResourceBindingLayout Single = Device->CreateBindingLayout(LayoutDesc);
	const FResourceBindingSet Recovered = Device->CreateBindingSet({Single, {{0, {Sampler}}}});
	HYP_CHECK(Recovered);
	const FSampler Foreign = InFixture.Device->CreateSampler({});
	Rejects(
	    [&]
	    {
		    Device->CreateBindingSet({Single, {{0, {Foreign}}}});
	    });
	FSamplerDesc Comparison;
	Comparison.bComparison = true;
	HYP_CHECK(InFixture.Device->CreateSampler(Comparison));
	HYP_CHECK(Device->Statistics().ValidationErrors == 0);
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		for (std::uint32_t Count : {0U, 1U, 8U})
		{
			CheckPixels(Fixture, Count);
		}
		CheckAllocationRollback(Fixture);
		CheckIntegerTextureRejection(Fixture);
		RunMaterialStateGpuTests(*Fixture.Device, *Fixture.Swapchain);
		RunShadowDepthTests(*Fixture.Device, *Fixture.Swapchain);
		RunMaterialPackingGpuTests(*Fixture.Device, *Fixture.Swapchain);
		RunMaterialCacheTests(*Fixture.Device, *Fixture.Swapchain);
		RunMaterialGpuCacheTests(*Fixture.Device, *Fixture.Swapchain);
		RunMaterialResourceTests(*Fixture.Device);
		RunMaterialSessionTests(*Fixture.Device, *Fixture.Swapchain);
		RunMaterialFrequencyTests(*Fixture.Device, *Fixture.Swapchain);
		std::cout << "PASS: material resource bindings, constant publication, descriptor rollback and GPU pixels\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
