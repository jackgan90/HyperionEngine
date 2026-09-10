#include "Hyperion/Renderer/MaterialGpuCache.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <fstream>

using namespace Hyperion;

namespace
{
FCompiledMaterialDefinition CompileResources()
{
	const auto Root = std::filesystem::absolute("material-gpu-cache-test/source");
	std::filesystem::create_directories(Root);
	std::ofstream(Root / "Sampler.hlsl") << R"(
Texture2D Source : register(t4, space1);
SamplerState Filter : register(s3);
float4 VSMain(float2 InPosition : POSITION) : SV_Position { return float4(InPosition, .5, 1); }
float4 PSMain() : SV_Target0 { return Source.SampleLevel(Filter, float2(1.2, .5), 0); }
)";
	FShaderCompiler Compiler(Root, "material-gpu-cache-test/cache");
	FMaterialDescription Description;
	Description.Name = "Dynamic sampling";
	FMaterialPass Pass;
	Pass.Vertex = {"Sampler.hlsl", "VSMain"};
	Pass.Pixel = {"Sampler.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	return CompileMaterialDefinition(Compiler, std::make_shared<const FMaterialDefinition>(Description),
	                                 EShaderFormat::Dxil);
}

FImage Render(IRHISwapchain& InSwapchain, FDrawPacket InDraw)
{
	InSwapchain.BeginFrame({64, 64});
	FPassCommands Pass;
	Pass.Color = FColorAttachment{FRenderTarget::Backbuffer()};
	Pass.Name = "Cached material resources";
	Pass.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::Present, EResourceState::RenderTarget}};
	Pass.Color->Actions.Load = EAttachmentLoad::Clear;
	Pass.Draws = {std::move(InDraw)};
	FPassCommands Present;
	Present.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::RenderTarget, EResourceState::Present}};
	const std::array Lists{InSwapchain.Record(0, Pass), InSwapchain.Record(1, Present)};
	return InSwapchain.EndFrame(Lists, false, true);
}

FDrawPacket Geometry(IRHIDevice& InDevice)
{
	FDrawPacket Draw;
	const std::array<float, 6> Vertices{-1, -1, 3, -1, -1, 3};
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	Draw.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Vertices)));
	Draw.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Draw.VertexStride = 8;
	Draw.IndexCount = 3;
	Draw.Scissor = {0, 0, 64, 64};
	return Draw;
}

void CheckReadBufferViews(IRHIDevice& InDevice)
{
	FMaterialGpuCache Cache(InDevice);
	FMaterialResourceOwners Owners{std::make_shared<const int>(1)};
	const std::array<std::uint32_t, 8> Data{1, 2, 3, 4, 5, 6, 7, 8};
	const auto Source = std::make_shared<const FMaterialReadBufferSource>(std::as_bytes(std::span(Data)));
	FCompiledMaterialPass Program;
	FMaterialProgramBinding Binding;
	Binding.Resource.Kind = EBindingKind::StructuredBuffer;
	Binding.Resource.Dimension = EShaderResourceDimension::Buffer;
	Binding.Resource.Register = 5;
	Binding.Resource.Space = 1;
	Binding.Stages = 2;
	Binding.ResourceParameter = 0;
	Program.Bindings.push_back(Binding);
	std::vector<std::optional<FMaterialValue>> Values{
	    FMaterialValue::FromBuffer({Source, EMaterialBufferViewKind::Structured, 0, 16, 4})};
	auto First = Cache.BindResources(Program, Values, Owners);
	Values[0] = FMaterialValue::FromBuffer({Source, EMaterialBufferViewKind::Structured, 16, 16, 4});
	auto Offset = Cache.BindResources(Program, Values, Owners);
	Values[0] = FMaterialValue::FromBuffer({Source, EMaterialBufferViewKind::Structured, 16, 16, 8});
	auto Stride = Cache.BindResources(Program, Values, Owners);
	HYP_CHECK(First.Set != Offset.Set && Offset.Set != Stride.Set && Cache.Statistics().ReadBufferUploads == 1);
	Program.Bindings[0].Resource.StructureByteStride = 8;
	auto Typed = Cache.BindResources(Program, Values, Owners);
	HYP_CHECK(Typed.Layout != Stride.Layout);
	Program.Bindings[0].Resource.StructureByteStride = 4;
	bool bRejected{};
	try
	{
		Cache.BindResources(Program, Values, Owners);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Typed = {};
	First = {};
	Offset = {};
	Stride = {};
	Owners.clear();
	Cache.Collect();
	HYP_CHECK(Cache.IsEmpty());
}

void CheckResourceArrayOrder(IRHIDevice& InDevice)
{
	FMaterialGpuCache Cache(InDevice);
	FMaterialResourceOwners Owners{std::make_shared<const int>(1)};
	FCompiledMaterialPass Program;
	FMaterialProgramBinding Binding;
	Binding.Resource.Kind = EBindingKind::Sampler;
	Binding.Resource.Count = 2;
	Binding.Stages = 2;
	Binding.ResourceParameter = 0;
	Program.Bindings.push_back(Binding);
	FMaterialSampler Clamp;
	Clamp.U = EMaterialAddressMode::Clamp;
	const auto A = FMaterialValue::FromSampler({});
	const auto B = FMaterialValue::FromSampler(Clamp);
	std::vector<std::optional<FMaterialValue>> Values{FMaterialValue::Array({A, B})};
	auto First = Cache.BindResources(Program, Values, Owners);
	Values[0] = FMaterialValue::Array({B, A});
	auto Reversed = Cache.BindResources(Program, Values, Owners);
	HYP_CHECK(First.Set != Reversed.Set && First.Layout == Reversed.Layout);
	Values[0] = FMaterialValue::Array({A, B});
	HYP_CHECK(Cache.BindResources(Program, Values, Owners).Set == First.Set);
	Program.Bindings[0].Resource.Space = 1;
	auto OtherLayout = Cache.BindResources(Program, Values, Owners);
	HYP_CHECK(OtherLayout.Set != First.Set && OtherLayout.Layout != First.Layout);
	HYP_CHECK(Cache.Statistics().SamplersCreated == 2 && Cache.Statistics().SetsCreated == 3);
	First = {};
	Reversed = {};
	OtherLayout = {};
	Owners.clear();
	Cache.Collect();
	HYP_CHECK(Cache.IsEmpty());
}

void CheckSharedOwners(IRHIDevice& InDevice)
{
	FMaterialGpuCache Cache(InDevice);
	FCompiledMaterialPass Program;
	FMaterialProgramBinding Binding;
	Binding.Resource.Kind = EBindingKind::Sampler;
	Binding.Stages = 2;
	Binding.ResourceParameter = 0;
	Program.Bindings.push_back(Binding);
	const std::array<std::optional<FMaterialValue>, 1> Values{FMaterialValue::FromSampler({})};
	const auto Shared = std::make_shared<const int>(0);
	FMaterialResourceOwners First{Shared, std::make_shared<const int>(1)};
	FMaterialResourceOwners Second{Shared, std::make_shared<const int>(2)};
	auto A = Cache.BindResources(Program, Values, First);
	auto B = Cache.BindResources(Program, Values, Second);
	HYP_CHECK(A.Set == B.Set);
	HYP_CHECK(Cache.BindResources(Program, Values, Second).Set == B.Set);
	A = {};
	B = {};
	First.clear();
	Cache.Collect();
	HYP_CHECK(!Cache.IsEmpty());
	Second.clear();
	Cache.Collect();
	HYP_CHECK(Cache.IsEmpty());
}

void CheckSampling(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	FMaterialGpuCache Cache(InDevice);
	const auto Program = CompileResources();
	FMaterialInstance Instance(Program.Interface);
	const auto Texture = std::make_shared<const FMaterialTextureSource>(
	    EMaterialTextureEncoding::Linear, std::vector<FMaterialTextureMip>{{2, 1, {204, 0, 0, 255, 0, 0, 204, 255}}});
	Instance.Set("Source", FMaterialValue::FromTexture(Texture));
	FMaterialSampler Sampler;
	Sampler.U = EMaterialAddressMode::Clamp;
	Sampler.bMinLinear = Sampler.bMagLinear = Sampler.bMipLinear = false;
	Instance.Set("Filter", FMaterialValue::FromSampler(Sampler));
	FMaterialResourceOwners Owners{std::make_shared<const int>(1)};
	FMaterialResourceOwners PipelineOwners{std::make_shared<const int>(2)};
	const auto Bind = [&]
	{
		const auto Parameters = ResolveMaterialBindingContext(Instance.Freeze(), Program, Program.GetPass(), {});
		return Cache.BindResources(Program.GetPass(), Parameters.Values, Owners);
	};
	auto Bindings = Bind();
	InDevice.WaitIdle();
	Bindings = Bind();
	HYP_CHECK(Bindings.bReady && Cache.Statistics().TextureUploads == 1 && Cache.Statistics().SetReuses == 1);
	auto Description = DescribeMaterialPipeline(Program.GetPass(), Instance.Freeze()->Definition->GetPass(),
	                                            Bindings.Layout, {{"POSITION", 0, EVertexFormat::Float2, 0}}, 8,
	                                            ERHIPrimitiveTopology::TriangleList, {});
	auto Draw = Geometry(InDevice);
	Draw.Pipeline = Cache.GetPipeline(Description, PipelineOwners);
	Draw.Bindings = Bindings.Set;
	const auto Clamp = Render(InSwapchain, Draw);
	const std::size_t Center = (32 * Clamp.Width + 32) * 4;
	HYP_CHECK(Clamp.Rgba[Center] < .01F && std::abs(Clamp.Rgba[Center + 2] - .8F) < .01F);
	const auto Before = InDevice.Statistics();
	Bindings = Bind();
	HYP_CHECK(Bindings.Set == Draw.Bindings && Cache.GetPipeline(Description, PipelineOwners) == Draw.Pipeline);
	HYP_CHECK(InDevice.Statistics().DescriptorCopies == Before.DescriptorCopies);
	Owners = {std::make_shared<const int>(3)};
	Sampler.U = EMaterialAddressMode::Repeat;
	Instance.Set("Filter", FMaterialValue::FromSampler(Sampler));
	Bindings = Bind();
	HYP_CHECK(Bindings.Set != Draw.Bindings && Cache.GetPipeline(Description, PipelineOwners) == Draw.Pipeline);
	Draw.Bindings = Bindings.Set;
	const auto Repeat = Render(InSwapchain, Draw);
	HYP_CHECK(std::abs(Repeat.Rgba[Center] - .8F) < .01F && Repeat.Rgba[Center + 2] < .01F);
	HYP_CHECK(Cache.Statistics().TextureUploads == 1 && Cache.Statistics().PipelinesCreated == 1 &&
	          Cache.Statistics().SetsCreated == 2);
	Description.State.ColorWriteMask = 1;
	auto ChangedState = Cache.GetPipeline(Description, PipelineOwners);
	HYP_CHECK(ChangedState != Draw.Pipeline);
	Description.Target.bSrgb = true;
	auto ChangedTarget = Cache.GetPipeline(Description, PipelineOwners);
	HYP_CHECK(ChangedTarget != ChangedState);
	Draw = {};
	Bindings = {};
	ChangedState = {};
	ChangedTarget = {};
	Description = {};
	Owners.clear();
	PipelineOwners.clear();
	InDevice.WaitIdle();
	InDevice.CollectCompletedResources();
	Cache.Collect();
	HYP_CHECK(Cache.IsEmpty());
}
} // namespace

void RunMaterialGpuCacheTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	CheckSampling(InDevice, InSwapchain);
	CheckReadBufferViews(InDevice);
	CheckResourceArrayOrder(InDevice);
	CheckSharedOwners(InDevice);
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}
