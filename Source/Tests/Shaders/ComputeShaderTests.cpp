#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <chrono>
#include <fstream>

namespace
{
using namespace Hyperion;

const FShaderBinding& Binding(const FShaderArtifact& InArtifact, std::string_view InName)
{
	const auto Found = std::find_if(InArtifact.Bindings.begin(), InArtifact.Bindings.end(),
	                                [&](const auto& InBinding)
	                                {
		                                return InBinding.Name == InName;
	                                });
	HYP_CHECK(Found != InArtifact.Bindings.end());
	return *Found;
}

void CheckReflection(const FShaderArtifact& InArtifact)
{
	const std::array<std::uint32_t, 3> Group{8, 4, 2};
	HYP_CHECK(InArtifact.Stage == EShaderStage::Compute && InArtifact.Reflection.ThreadGroupSize == Group);
	HYP_CHECK(Binding(InArtifact, "Output").Kind == EBindingKind::StorageTexture);
	HYP_CHECK(Binding(InArtifact, "Output").Dimension == EShaderResourceDimension::Texture2D);
	HYP_CHECK(Binding(InArtifact, "Output").Register == 3 && Binding(InArtifact, "Output").Space == 2);
	HYP_CHECK(Binding(InArtifact, "Records").Kind == EBindingKind::StorageStructuredBuffer);
	HYP_CHECK(Binding(InArtifact, "Records").StructureByteStride == 16);
	HYP_CHECK(Binding(InArtifact, "RawOutput").Kind == EBindingKind::StorageRawBuffer);
	HYP_CHECK(Binding(InArtifact, "Source").Kind == EBindingKind::Texture);
	HYP_CHECK(Binding(InArtifact, "ReadRecords").Kind == EBindingKind::StructuredBuffer);
	HYP_CHECK(Binding(InArtifact, "ReadRaw").Kind == EBindingKind::RawBuffer);
	HYP_CHECK(Binding(InArtifact, "Parameters").ByteSize >= 16);
	const bool bDxil = InArtifact.Format == EShaderFormat::Dxil;
	HYP_CHECK(Binding(InArtifact, "ReadRecords").Binding == (bDxil ? 5U : 1005U));
	HYP_CHECK(Binding(InArtifact, "ReadRaw").Binding == (bDxil ? 6U : 1006U));
	HYP_CHECK(Binding(InArtifact, "Records").Binding == (bDxil ? 5U : 3005U));
	HYP_CHECK(Binding(InArtifact, "RawOutput").Binding == (bDxil ? 6U : 3006U));
	for (const auto& Resource : InArtifact.Bindings)
	{
		HYP_CHECK(Resource.Stage == EShaderStage::Compute);
		if (InArtifact.Format == EShaderFormat::Msl)
		{
			HYP_CHECK(Resource.MslBinding != 0xffffffffU);
		}
	}
}

void CheckFixedRegisterBindings(const FShaderArtifact& InArtifact, std::uint32_t InSpace)
{
	const bool bDxil = InArtifact.Format == EShaderFormat::Dxil;
	const std::array<std::pair<const char*, std::uint32_t>, 4> Expected{
	    {{"Parameters", 7}, {"Source", 1007}, {"Comparison", 2007}, {"Output", 3007}}};
	for (const auto& [Name, TargetBinding] : Expected)
	{
		const auto& Resource = Binding(InArtifact, Name);
		HYP_CHECK(Resource.Register == 7 && Resource.Space == InSpace && Resource.Count == 1);
		HYP_CHECK(Resource.Binding == (bDxil ? 7U : TargetBinding));
	}
	HYP_CHECK(Binding(InArtifact, "Parameters").Kind == EBindingKind::UniformBuffer);
	HYP_CHECK(Binding(InArtifact, "Source").Kind == EBindingKind::Texture);
	HYP_CHECK(Binding(InArtifact, "Comparison").Kind == EBindingKind::Sampler);
	HYP_CHECK(Binding(InArtifact, "Comparison").bComparison);
	HYP_CHECK(Binding(InArtifact, "Output").Kind == EBindingKind::StorageTexture);
	HYP_CHECK(Binding(InArtifact, "Cube").Dimension == EShaderResourceDimension::TextureCube);
	HYP_CHECK(Binding(InArtifact, "Cube").Binding == (bDxil ? 8U : 1008U));
	HYP_CHECK(!Binding(InArtifact, "Regular").bComparison);
	HYP_CHECK(Binding(InArtifact, "Regular").Binding == (bDxil ? 8U : 2008U));
}

void CheckFixedRegisterMappings(const std::filesystem::path& InRoot)
{
	std::ofstream(InRoot / "FixedRegisterMapping.hlsl") << R"(
cbuffer Parameters : register(b7, TEST_SPACE) { float Value; };
Texture2D<float> Source : register(t7, TEST_SPACE);
TextureCube<float4> Cube : register(t8, TEST_SPACE);
SamplerComparisonState Comparison : register(s7, TEST_SPACE);
SamplerState Regular : register(s8, TEST_SPACE);
RWTexture2D<float> Output : register(u7, TEST_SPACE);
[numthreads(1, 1, 1)] void CSMain(uint3 InId : SV_DispatchThreadID)
{
    Output[InId.xy] = Source.SampleCmpLevelZero(Comparison, float2(.25, .5), Value)
        + Cube.SampleLevel(Regular, float3(1, Value, 0), 0).x;
}
)";
	const auto Identity = std::chrono::steady_clock::now().time_since_epoch().count();
	FShaderCompiler Compiler(InRoot, std::filesystem::path("compute-shader-test/mapping") / std::to_string(Identity));
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		for (std::uint32_t Space = 0; Space != 4; ++Space)
		{
			const FShaderCompileOptions Options{{{"TEST_SPACE", "space" + std::to_string(Space)}}};
			const auto Cold =
			    Compiler.Compile("FixedRegisterMapping.hlsl", "CSMain", EShaderStage::Compute, Format, Options);
			CheckFixedRegisterBindings(Cold, Space);
			const auto Hot =
			    Compiler.Compile("FixedRegisterMapping.hlsl", "CSMain", EShaderStage::Compute, Format, Options);
			HYP_CHECK(Hot.bCacheHit && Hot.Bytes == Cold.Bytes && Hot.Bindings == Cold.Bindings &&
			          Hot.Reflection == Cold.Reflection);
		}
	}
}
} // namespace

void CheckComputeShaders(const std::filesystem::path& InRoot)
{
	using namespace Hyperion;
	CheckFixedRegisterMappings(InRoot);
	{
		std::ofstream Source(InRoot / "ComputeReflection.hlsl");
		Source << R"(
cbuffer Parameters : register(b1, space1) { float Scale; float3 Offset; };
Texture2D<float> Source : register(t3, space2);
SamplerState PointSampler : register(s2, space2);
StructuredBuffer<float4> ReadRecords : register(t5, space1);
ByteAddressBuffer ReadRaw : register(t6, space1);
RWTexture2D<float> Output : register(u3, space2);
RWStructuredBuffer<float4> Records : register(u5, space1);
RWByteAddressBuffer RawOutput : register(u6, space1);
[numthreads(8, 4, 2)]
void CSMain(uint3 InId : SV_DispatchThreadID)
{
    float Value = Source.SampleLevel(PointSampler, (float2(InId.xy) + .5) / 64, 0) * Scale * FACTOR;
    Output[InId.xy] = Value + Offset[InId.z % 3];
    Records[InId.x] = ReadRecords[InId.x] + Value;
    RawOutput.Store(InId.x * 4, ReadRaw.Load(InId.x * 4) + InId.z);
}
)";
	}
	const auto Identity = std::chrono::steady_clock::now().time_since_epoch().count();
	FShaderCompiler Compiler(InRoot, std::filesystem::path("compute-shader-test/cache") / std::to_string(Identity));
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const FShaderCompileOptions Options{{{"FACTOR", "1"}}};
		const auto Cold = Compiler.Compile("ComputeReflection.hlsl", "CSMain", EShaderStage::Compute, Format, Options);
		CheckReflection(Cold);
		const auto Warm = Compiler.Compile("ComputeReflection.hlsl", "CSMain", EShaderStage::Compute, Format, Options);
		HYP_CHECK(Warm.bCacheHit && Warm.Bytes == Cold.Bytes && Warm.Bindings == Cold.Bindings &&
		          Warm.Reflection == Cold.Reflection);
		const auto Changed =
		    Compiler.Compile("ComputeReflection.hlsl", "CSMain", EShaderStage::Compute, Format, {{{"FACTOR", "2"}}});
		HYP_CHECK(Changed.CacheKey != Warm.CacheKey && Changed.Bytes != Warm.Bytes);
	}
	bool bRejected = false;
	try
	{
		Compiler.Compile("ComputeReflection.hlsl", "CSMain", EShaderStage(99), EShaderFormat::Dxil);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
