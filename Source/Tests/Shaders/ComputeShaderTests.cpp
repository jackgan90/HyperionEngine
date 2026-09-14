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
	for (const auto& Resource : InArtifact.Bindings)
	{
		HYP_CHECK(Resource.Stage == EShaderStage::Compute);
		if (InArtifact.Format == EShaderFormat::Msl)
		{
			HYP_CHECK(Resource.MslBinding != 0xffffffffU);
		}
	}
}
} // namespace

void CheckComputeShaders(const std::filesystem::path& InRoot)
{
	using namespace Hyperion;
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
