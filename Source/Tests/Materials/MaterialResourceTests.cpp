#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <fstream>
#include <thread>

using namespace Hyperion;

namespace
{
void WaitFor(const std::function<bool()>& InCondition)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (!InCondition())
	{
		if (std::chrono::steady_clock::now() > Deadline)
		{
			throw std::runtime_error("Material resource progress timed out");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void CheckZeroFrameReadiness(IRHIDevice& InDevice)
{
	const auto Root = std::filesystem::absolute("material-resource-test/source");
	std::filesystem::create_directories(Root);
	std::ofstream(Root / "Ready.hlsl") << R"(
cbuffer ViewData : register(b0) { float4x4 ViewProjection; };
cbuffer ObjectData : register(b1) { float4x4 World; };
cbuffer Surface : register(b2) { float4 Tint; };
float4 VSMain(float3 InPosition : POSITION) : SV_Position { return mul(ViewProjection, mul(World, float4(InPosition, 1))); }
float4 PSMain() : SV_Target0 { return Tint; }
)";
	FTaskSystem Tasks(1, 1);
	FShaderCompiler Compiler(Root, "material-resource-test/cache");
	FRenderResourceService Service(Tasks, InDevice, Compiler);
	FMaterialDescription Description;
	Description.Name = "Resources before a view exists";
	FMaterialPass Pass;
	Pass.Vertex = {"Ready.hlsl", "VSMain"};
	Pass.Pixel = {"Ready.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	const auto Registry = GetStandardMaterialSemantics();
	Description.Parameters = {DeclareMaterialSemantic("ViewProjection", "Engine.View.ViewProjection", *Registry),
	                          DeclareMaterialSemantic("World", "Engine.Object.World", *Registry)};
	FMaterialParameterDeclaration Tint;
	Tint.Name = "Tint";
	Tint.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float, 4);
	Tint.Default = FMaterialValue::Float(FVec4{1, 1, 1, 1});
	Description.Parameters.push_back(Tint);
	FMaterialInstance Instance(std::make_shared<const FMaterialDefinition>(Description));
	const auto Before = InDevice.Statistics().SubmittedFrames;
	auto First = Service.RequestMaterial(Instance.Freeze());
	WaitFor(
	    [&]
	    {
		    return First->GetStatus() == ERenderMaterialStatus::Ready ||
		           First->GetStatus() == ERenderMaterialStatus::Failed;
	    });
	HYP_CHECK(First->GetStatus() == ERenderMaterialStatus::Ready && First->IsInterfaceReady());
	HYP_CHECK(InDevice.Statistics().SubmittedFrames == Before);
	HYP_CHECK(Service.Statistics().Constants.PagesCreated == 0 && Service.Statistics().Materials.PipelinesCreated == 0);
	Instance.Set("Tint", FMaterialValue::Float(FVec4{.5F, .6F, .7F, 1}));
	auto Edited = Service.RequestMaterial(Instance.Freeze());
	HYP_CHECK(Edited->GetStatus() == ERenderMaterialStatus::Ready && Edited->GetCompiled() == First->GetCompiled());
	HYP_CHECK(Edited->GetSnapshot()->Revision != First->GetSnapshot()->Revision);
	First.reset();
	Edited.reset();
	WaitFor(
	    [&]
	    {
		    return Service.Statistics().Materials.LiveObjects == 0;
	    });
	Service.Close();
}

FRenderResourceDesc PreparedGeometry(std::shared_ptr<const FMaterialSnapshot> InSurface,
                                     std::shared_ptr<const FCompiledMaterialDefinition> InCompiled)
{
	FRenderResourceDesc Result;
	FRenderGeometryDesc Geometry;
	const std::array<FVec3, 3> Vertices{{{-1, -1, .5f}, {3, -1, .5f}, {-1, 3, .5f}}};
	const auto Bytes = std::as_bytes(std::span(Vertices));
	Geometry.Vertices.assign(Bytes.begin(), Bytes.end());
	Geometry.Indices = {0, 1, 2};
	Geometry.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
	Geometry.VertexStride = sizeof(FVec3);
	Result.Geometries.push_back(std::move(Geometry));
	Result.Materials.push_back({std::move(InSurface), std::move(InCompiled)});
	Result.Sections.push_back({0, 0, 0, 3});
	return Result;
}

std::shared_ptr<const FMaterialDefinition> PreparedDefinition(const std::filesystem::path& InRoot)
{
	std::filesystem::create_directories(InRoot);
	std::ofstream(InRoot / "Prepared.hlsl") << R"(
#ifndef COLOR
#define COLOR .5
#endif
cbuffer Surface : register(b0) { float Gain; };
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position, 1); }
float4 PSMain() : SV_Target0 { return float4(COLOR * Gain, 0, 0, 1); }
)";
	FMaterialDescription Description;
	Description.Name = "Prepared program selection";
	FMaterialPass Pass;
	Pass.Vertex = {"Prepared.hlsl", "VSMain"};
	Pass.Pixel = {"Prepared.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	FMaterialParameterDeclaration Gain;
	Gain.Name = "Gain";
	Gain.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float);
	Gain.Default = FMaterialValue::Float(1);
	Description.Parameters.push_back(Gain);
	return std::make_shared<const FMaterialDefinition>(Description);
}

void CheckPreparedPrograms(IRHIDevice& InDevice)
{
	const auto Root = std::filesystem::absolute("material-prepared-selection/source");
	const auto Definition = PreparedDefinition(Root);
	FShaderCompiler Compiler(Root, "material-prepared-selection/cache");
	auto A = std::make_shared<const FCompiledMaterialDefinition>(CompileMaterialDefinition(
	    Compiler, Definition, EShaderFormat::Dxil, {{"Forward", "Default", {{"COLOR", ".25"}}}}));
	auto B = std::make_shared<const FCompiledMaterialDefinition>(CompileMaterialDefinition(
	    Compiler, Definition, EShaderFormat::Dxil, {{"Forward", "Default", {{"COLOR", ".75"}}}}));
	HYP_CHECK(A->Key != B->Key && A->GetPass().Pixel.Bytes != B->GetPass().Pixel.Bytes);
	const std::weak_ptr<const FCompiledMaterialDefinition> WeakA = A;
	const std::weak_ptr<const FCompiledMaterialDefinition> WeakB = B;
	FTaskSystem Tasks(1, 1);
	FRenderResourceService Service(Tasks, InDevice, Compiler);
	const auto Request = [&](const std::shared_ptr<const FCompiledMaterialDefinition>& InProgram,
	                         std::shared_ptr<const FMaterialSnapshot> InSnapshot)
	{
		return Service.Request(std::make_shared<const int>(0), 1, "prepared-selection",
		                       [InProgram, Snapshot = std::move(InSnapshot)]
		                       {
			                       return PreparedGeometry(Snapshot, InProgram);
		                       });
	};
	auto First = Request(A, FMaterialInstance(A->Interface).Freeze());
	WaitFor(
	    [&]
	    {
		    return First->GetStatus() == ERenderResourceStatus::Ready &&
		           First->GetMaterial(0)->GetStatus() == ERenderMaterialStatus::Ready;
	    });
	FMaterialInstance Instance(B->Interface);
	auto Second = Request(B, Instance.Freeze());
	WaitFor(
	    [&]
	    {
		    return Second->GetStatus() == ERenderResourceStatus::Ready &&
		           Second->GetMaterial(0)->GetStatus() == ERenderMaterialStatus::Ready;
	    });
	HYP_CHECK(First->GetMaterial(0)->GetCompiled() == A && Second->GetMaterial(0)->GetCompiled() == B);
	Instance.Set("Gain", FMaterialValue::Float(.5f));
	auto Edited = Request(B, Instance.Freeze());
	WaitFor(
	    [&]
	    {
		    return Edited->GetStatus() == ERenderResourceStatus::Ready &&
		           Edited->GetMaterial(0)->GetStatus() == ERenderMaterialStatus::Ready;
	    });
	HYP_CHECK(Edited->GetMaterial(0)->GetCompiled() == B);
	HYP_CHECK(Edited->GetMaterial(0)->GetSnapshot()->Revision != Second->GetMaterial(0)->GetSnapshot()->Revision);
	auto Automatic = Service.RequestMaterial(FMaterialInstance(Definition).Freeze());
	WaitFor(
	    [&]
	    {
		    return Automatic->GetStatus() == ERenderMaterialStatus::Ready;
	    });
	HYP_CHECK(Automatic->GetCompiled()->GetPass().Pixel.Bytes != A->GetPass().Pixel.Bytes &&
	          Automatic->GetCompiled()->GetPass().Pixel.Bytes != B->GetPass().Pixel.Bytes);
	const auto Foreign = std::make_shared<const FCompiledMaterialDefinition>(
	    CompileMaterialDefinition(Compiler, Definition, EShaderFormat::Spirv));
	auto Rejected = Request(Foreign, FMaterialInstance(Foreign->Interface).Freeze());
	WaitFor(
	    [&]
	    {
		    return Rejected->GetStatus() == ERenderResourceStatus::Failed;
	    });
	HYP_CHECK(Rejected->GetError().find("format") != std::string::npos);
	First.reset();
	Second.reset();
	Edited.reset();
	Automatic.reset();
	Rejected.reset();
	A.reset();
	B.reset();
	WaitFor(
	    [&]
	    {
		    return WeakA.expired() && WeakB.expired() && Service.Statistics().Materials.LiveObjects == 0;
	    });
	Service.Close();
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}

void CheckUploadThenDescriptorFailure()
{
	const auto Root = std::filesystem::absolute("material-resource-failure/source");
	std::filesystem::create_directories(Root);
	std::ofstream(Root / "Failure.hlsl") << R"(
Texture2D Maps[2] : register(t0);
SamplerState Filter : register(s0);
float4 VSMain(float2 InPosition : POSITION) : SV_Position { return float4(InPosition, .5, 1); }
float4 PSMain() : SV_Target0 { return Maps[0].Sample(Filter, .5) + Maps[1].Sample(Filter, .5); }
)";
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	FRHIDeviceDesc DeviceDescription;
	DeviceDescription.ResourceDescriptorCapacity = 1;
	auto Device = Registry.CreateDevice(ERHIBackend::D3D12, DeviceDescription);
	FTaskSystem Tasks(1, 1);
	FShaderCompiler Compiler(Root, "material-resource-failure/cache");
	FRenderResourceService Service(Tasks, *Device, Compiler);
	FMaterialDescription Description;
	Description.Name = "Upload then exhausted visible descriptors";
	FMaterialPass Pass;
	Pass.Vertex = {"Failure.hlsl", "VSMain"};
	Pass.Pixel = {"Failure.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	const auto Source = std::make_shared<const FMaterialTextureSource>(
	    EMaterialTextureEncoding::Linear, std::vector<FMaterialTextureMip>{{1, 1, {255, 255, 255, 255}}});
	FMaterialParameterDeclaration Maps;
	Maps.Name = "Maps";
	Maps.Type = FMaterialParameterType::Array(FMaterialParameterType::Resource(EMaterialValueKind::Texture2D), 2);
	Maps.Default = FMaterialValue::Array({FMaterialValue::FromTexture(Source), FMaterialValue::FromTexture(Source)});
	Description.Parameters.push_back(Maps);
	FMaterialParameterDeclaration Filter;
	Filter.Name = "Filter";
	Filter.Type = FMaterialParameterType::Resource(EMaterialValueKind::Sampler);
	Filter.Default = FMaterialValue::FromSampler({});
	Description.Parameters.push_back(Filter);
	FMaterialInstance Instance(std::make_shared<const FMaterialDefinition>(Description));
	auto Material = Service.RequestMaterial(Instance.Freeze());
	WaitFor(
	    [&]
	    {
		    return Material->GetStatus() == ERenderMaterialStatus::Failed;
	    });
	HYP_CHECK(Material->IsInterfaceReady() && !Material->GetError().empty());
	WaitFor(
	    [&]
	    {
		    return Service.Statistics().Materials.LiveObjects == 0;
	    });
	HYP_CHECK(Service.Statistics().Materials.TextureUploads == 1);
	HYP_CHECK(Device->Statistics().SubmittedFrames == 0 && Device->Statistics().ValidationErrors == 0);
	Material.reset();
	Service.Close();
}
} // namespace

void RunMaterialResourceTests(IRHIDevice& InDevice)
{
	CheckZeroFrameReadiness(InDevice);
	CheckPreparedPrograms(InDevice);
	CheckUploadThenDescriptorFailure();
}
