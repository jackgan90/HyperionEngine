#include "Hyperion/Materials/Lighting/ClusterParameters.h"
#include "Hyperion/Materials/Lighting/EnvironmentParameters.h"
#include "Hyperion/Materials/Lighting/ShadowParameters.h"
#include "Hyperion/Materials/PbrMaterial.h"
#include "Hyperion/Materials/PbrParameters.h"
#include "Hyperion/Renderer/MaterialBlocks.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <fstream>

using namespace Hyperion;

namespace
{
FMaterialDescription Description(std::string InFile)
{
	FMaterialDescription Result;
	Result.Name = "Engine semantic contract";
	FMaterialPass Pass;
	Pass.Vertex = {InFile, "VSMain"};
	Pass.Pixel = {std::move(InFile), "PSMain"};
	Result.Passes.push_back(std::move(Pass));
	return Result;
}

void CheckIdentities()
{
	const auto Registry = GetStandardMaterialSemantics();
	for (unsigned Index = 1; Index < static_cast<unsigned>(EEngineSemantic::Count); ++Index)
	{
		const auto Id = static_cast<EEngineSemantic>(Index);
		const FMaterialSemanticId Identity(Id);
		HYP_CHECK(Identity.GetBuiltin() == Id && FMaterialSemanticId(Identity.GetName()) == Identity);
		HYP_CHECK(Registry->Normalize(Id) == Identity);
	}
	HYP_CHECK(FMaterialSemanticId("ALBEDO_TEXTURE") == EPbrSemantic::BaseColorTexture);
	HYP_CHECK(FMaterialSemanticId("Game.Time").GetBuiltin() == EEngineSemantic::None);
	const FMaterialDefinition First(Description("Unused.hlsl"));
	const FMaterialDefinition Second(Description("Unused.hlsl"));
	HYP_CHECK(&First.GetSemantics() == Registry.get() && &Second.GetSemantics() == Registry.get());
	auto Custom = std::make_shared<FMaterialSemanticRegistry>();
	Custom->Register({"Game.Wind",
	                  FMaterialParameterType::Numeric(EMaterialScalar::Float),
	                  EMaterialScope::Scene,
	                  "Wind strength",
	                  true,
	                  {"Game.WindAlias"}});
	const FMaterialDefinition Snapshot(Description("Unused.hlsl"), Custom);
	Custom->Register({"Game.Extra", FMaterialParameterType::Numeric(EMaterialScalar::Float), EMaterialScope::Global,
	                  "Extension after snapshot"});
	HYP_CHECK(Snapshot.GetSemantics().IsFrozen() && &Snapshot.GetSemantics() != Custom.get());
	HYP_CHECK(&Custom->Find(EEngineSemantic::Time) == &Registry->Find(EEngineSemantic::Time));
	HYP_CHECK(Snapshot.GetSemantics().Normalize("Game.WindAlias") == FMaterialSemanticId("Game.Wind"));
	HYP_CHECK(Snapshot.GetSemantics().GetVersion() + 1 == Custom->GetVersion());
	const auto Pbr = MakePbrMaterialAsset("PBR catalog");
	for (const auto& Parameter : Pbr.Parameters)
	{
		HYP_CHECK(FMaterialSemanticId(Parameter.Semantic).IsBuiltin());
		HYP_CHECK(Registry->Find(Parameter.Semantic).Scope == EMaterialScope::Material);
	}
}

void WriteShaders(const std::filesystem::path& InRoot)
{
	std::ofstream(InRoot / "Good.hlsl") << R"(
cbuffer FrameInfo : register(b7, space1) { float Time; uint Index; };
#ifndef HYP_CLUSTER_ALIAS
#define HYP_CLUSTER_ALIAS 1
#endif
#if HYP_CLUSTER_ALIAS
cbuffer ClusterViewInfo : register(b9, space2)
#else
cbuffer ClusterViewV1 : register(b9, space2)
#endif
{
    float4 ClusterViewport; float4 ClusterGrid; float4 ClusterCamera; float4 ClusterForward; float4 ClusterDepth;
};
struct FLight { float4 PositionRange; float4 RadianceType; float4 DirectionInner; float4 Outer; };
StructuredBuffer<FLight> ClusterLights : register(t13, space1);
StructuredBuffer<uint2> ClusterHeaders : register(t14, space1);
StructuredBuffer<uint> ClusterIndices : register(t15, space1);
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position, 1); }
float4 PSMain() : SV_Target0
{
    return Time + Index + ClusterViewport + ClusterGrid + ClusterCamera + ClusterForward + ClusterDepth +
           ClusterLights[0].PositionRange + ClusterLights[0].RadianceType + ClusterLights[0].DirectionInner +
           ClusterLights[0].Outer + ClusterHeaders[0].x + ClusterIndices[0];
}
)";
	std::ofstream(InRoot / "Unused.hlsl") << R"(
StructuredBuffer<float> ClusterLights : register(t0);
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position,1); }
float4 PSMain() : SV_Target0 { return 1; }
)";
	std::ofstream(InRoot / "BadScalar.hlsl") << R"(
struct FLight { uint4 PositionRange; float4 RadianceType; float4 DirectionInner; float4 Outer; };
StructuredBuffer<FLight> ClusterLights : register(t13);
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position,1); }
float4 PSMain() : SV_Target0 { return ClusterLights[0].PositionRange; }
)";
	std::ofstream(InRoot / "BadMatrix.hlsl") << R"(
cbuffer ViewInfo : register(b1) { row_major float4x4 ViewProjection; float3 CameraPosition; };
float4 VSMain(float3 Position : POSITION) : SV_Position { return mul(ViewProjection,float4(Position,1)); }
float4 PSMain() : SV_Target0 { return CameraPosition.x; }
)";
	std::ofstream(InRoot / "BadArray.hlsl") << R"(
cbuffer FrameInfo : register(b0) { float Time[1]; uint Index; };
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position,1); }
float4 PSMain() : SV_Target0 { return Time[0] + Index; }
)";
	std::ofstream(InRoot / "BadTexture.hlsl") << R"(
Texture2D<float4> EnvironmentSpecular : register(t0);
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position,1); }
float4 PSMain() : SV_Target0 { return EnvironmentSpecular.Load(int3(0,0,0)); }
)";
	std::ofstream(InRoot / "BadOrder.hlsl") << R"(
struct FLight { float4 RadianceType; float4 PositionRange; float4 DirectionInner; float4 Outer; };
StructuredBuffer<FLight> ClusterLights : register(t0);
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position,1); }
float4 PSMain() : SV_Target0 { return ClusterLights[0].PositionRange; }
)";
	std::ofstream(InRoot / "BadInactive.hlsl") << R"(
cbuffer FrameInfo : register(b0) { float Time; float Index; };
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position,1); }
float4 PSMain() : SV_Target0 { return Time; }
)";
	std::ofstream(InRoot / "BadKind.hlsl") << R"(
ByteAddressBuffer ClusterIndices : register(t0);
float4 VSMain(float3 Position : POSITION) : SV_Position { return float4(Position,1); }
float4 PSMain() : SV_Target0 { return ClusterIndices.Load(0); }
)";
}

void CheckRejected(FShaderCompiler& InCompiler, EShaderFormat InFormat, const std::string& InFile,
                   std::string_view InReason)
{
	try
	{
		CompileMaterialDefinition(InCompiler, std::make_shared<const FMaterialDefinition>(Description(InFile)),
		                          InFormat);
	}
	catch (const std::invalid_argument& Error)
	{
		HYP_CHECK(std::string_view(Error.what()).find(InReason) != std::string_view::npos);
		return;
	}
	throw std::runtime_error("Engine contract accepted incompatible shader: " + InFile);
}

void CheckDiscovery(FShaderCompiler& InCompiler, EShaderFormat InFormat)
{
	const auto Definition = std::make_shared<const FMaterialDefinition>(Description("Good.hlsl"));
	const auto Program = CompileMaterialDefinition(InCompiler, Definition, InFormat);
	const auto& Schema = *Program.Interface.Schema;
	const auto Variants = CompileMaterialDefinition(
	    InCompiler, Definition, InFormat,
	    {{"Forward", "Alias", {{"HYP_CLUSTER_ALIAS", "1"}}}, {"Forward", "Canonical", {{"HYP_CLUSTER_ALIAS", "0"}}}});
	HYP_CHECK(Variants.Interface.Schema->GetParameters().size() == Schema.GetParameters().size());
	HYP_CHECK(Schema.Get(Schema.FindSemantic(EEngineSemantic::Time)).Semantic == EEngineSemantic::Time);
	HYP_CHECK(!Schema.Get(Schema.FindSemantic(EEngineSemantic::Time)).Default);
	HYP_CHECK(Schema.Get(Schema.FindSemantic(EClusterSemantic::ClusterLights)).Semantic ==
	          EClusterSemantic::ClusterLights);
	HYP_CHECK(Schema.Get(Schema.FindSemantic(EClusterSemantic::ClusterLights)).Default.has_value());
	HYP_CHECK(std::none_of(Schema.GetParameters().begin(), Schema.GetParameters().end(),
	                       [](const auto& InParameter)
	                       {
		                       return InParameter.Semantic == EEnvironmentV1Field::EnvironmentControl;
	                       }));
	for (const auto& Binding : Program.GetPass().Bindings)
	{
		if (Binding.Resource.Name == "ClusterLights")
		{
			HYP_CHECK(Binding.Resource.Register == 13 && Binding.Resource.Space == 1);
			HYP_CHECK(Binding.Resource.StructureByteStride == 64 && Binding.Resource.Members.size() == 1);
			HYP_CHECK(Binding.Resource.Members.front().Members.size() == 4);
		}
	}
	// A cache hit must preserve all element metadata.
	const auto First = InCompiler.Compile("Good.hlsl", "PSMain", EShaderStage::Pixel, InFormat);
	const auto Cached = InCompiler.Compile("Good.hlsl", "PSMain", EShaderStage::Pixel, InFormat);
	HYP_CHECK(Cached.bCacheHit && Cached.Bindings == First.Bindings);
	const auto Explicit =
	    CompileMaterialDefinition(InCompiler, Definition, InFormat, {}, EMaterialEngineBindingMode::Explicit);
	HYP_CHECK(Explicit.Key != Program.Key);
	FMaterialInstance Instance(Explicit.Interface);
	Instance.SetSemantic(EEngineSemantic::Time, FMaterialValue::Float(2));
	const auto Unused = CompileMaterialDefinition(
	    InCompiler, std::make_shared<const FMaterialDefinition>(Description("Unused.hlsl")), InFormat);
	HYP_CHECK(Unused.Interface.Schema->GetParameters().empty() && Unused.GetPass().Bindings.empty());
	CheckRejected(InCompiler, InFormat, "BadScalar.hlsl", "Engine resource contract mismatch");
	CheckRejected(InCompiler, InFormat, "BadMatrix.hlsl", "ABI mismatch");
	CheckRejected(InCompiler, InFormat, "BadArray.hlsl", "block");
	CheckRejected(InCompiler, InFormat, "BadTexture.hlsl", "Engine resource contract mismatch");
	CheckRejected(InCompiler, InFormat, "BadOrder.hlsl", "Engine resource contract mismatch");
	CheckRejected(InCompiler, InFormat, "BadInactive.hlsl", "ABI mismatch");
	CheckRejected(InCompiler, InFormat, "BadKind.hlsl", "Engine resource contract mismatch");
}

void CheckResourceErrors()
{
	FShaderBinding Sampler;
	Sampler.Name = "ShadowSampler";
	Sampler.Kind = EBindingKind::Sampler;
	const auto Contract = GetEngineMaterialResource(Sampler.Name);
	HYP_CHECK(Contract.Semantic == EShadowSemantic::ShadowSampler);
	bool bRejected = false;
	try
	{
		ValidateEngineMaterialResource(Sampler);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Sampler.bComparison = true;
	HYP_CHECK(ValidateEngineMaterialResource(Sampler).bComparison);
	Sampler.Count = 2;
	bRejected = false;
	try
	{
		ValidateEngineMaterialResource(Sampler);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckStructuredShape(FShaderCompiler& InCompiler, const std::filesystem::path& InRoot)
{
	std::ofstream(InRoot / "StructuredShape.hlsl") << R"(
struct FRecord { float Samples[3]; column_major float3x2 Basis; };
StructuredBuffer<FRecord> Records : register(t0);
float4 PSMain() : SV_Target0 { return Records[0].Samples[2] + Records[0].Basis[0][1]; }
)";
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto Shader = InCompiler.Compile("StructuredShape.hlsl", "PSMain", EShaderStage::Pixel, Format);
		const auto& Binding = Shader.Bindings.at(0);
		const auto& Element = Binding.Members.at(0);
		const auto& Samples = Element.Members.at(0);
		const auto& Basis = Element.Members.at(1);
		const bool bDxil = Format == EShaderFormat::Dxil;
		const std::uint32_t ExpectedStride = bDxil ? 36 : 48;
		HYP_CHECK(Binding.StructureByteStride == ExpectedStride && Element.Size == ExpectedStride);
		HYP_CHECK(Samples.ArrayCount == 3 && Samples.ArrayStride == 4 && Samples.Size == 12);
		HYP_CHECK(Basis.Offset == (bDxil ? 12U : 16U) && Basis.MatrixStride == (bDxil ? 12U : 16U));
		HYP_CHECK(Basis.Size == (bDxil ? 24U : 28U));
		HYP_CHECK(Basis.Rows == 3 && Basis.Columns == 2 && !Basis.bRowMajor);
	}
}

void CheckNativeStructuredShape(FShaderCompiler& InCompiler, const std::filesystem::path& InRoot)
{
	std::ofstream(InRoot / "NativeShape.hlsl") << R"(
struct FLight
{
#ifdef __spirv__
    float3 PositionRange;
#else
    float4 PositionRange;
#endif
    float4 RadianceType; float4 DirectionInner; float4 Outer;
};
StructuredBuffer<FLight> ClusterLights : register(t0);
float4 PSMain() : SV_Target0 { return ClusterLights[0].PositionRange.x; }
)";
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto Shader = InCompiler.Compile("NativeShape.hlsl", "PSMain", EShaderStage::Pixel, Format);
		const auto& Binding = Shader.Bindings.at(0);
		HYP_CHECK(Binding.StructureByteStride == 64);
		const auto& Position = Binding.Members.at(0).Members.at(0);
		if (Format == EShaderFormat::Dxil)
		{
			HYP_CHECK(Position.Columns == 4 && Position.Size == 16);
			ValidateEngineMaterialResource(Binding);
		}
		else
		{
			HYP_CHECK(Position.Columns == 3 && Position.Size == 12);
			bool bRejected{};
			try
			{
				ValidateEngineMaterialResource(Binding);
			}
			catch (const std::invalid_argument&)
			{
				bRejected = true;
			}
			HYP_CHECK(bRejected);
		}
	}
}
} // namespace

void RunEngineSemanticTests()
{
	CheckIdentities();
	CheckResourceErrors();
	const auto Root = std::filesystem::absolute("engine-semantic-test/source");
	std::filesystem::create_directories(Root);
	WriteShaders(Root);
	FShaderCompiler Compiler(Root, "engine-semantic-test/cache");
	CheckStructuredShape(Compiler, Root);
	CheckNativeStructuredShape(Compiler, Root);
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		CheckDiscovery(Compiler, Format);
	}
}
