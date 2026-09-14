#pragma once
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/IOService.h"

namespace Hyperion
{
// Test-only source generation keeps compiler/RHI contracts independent of sample repositories.
inline const std::filesystem::path& TestShaderRoot()
{
	static const auto Root = []
	{
		const auto Directory = std::filesystem::absolute("shader-fixtures") / CreateIdentifier();
		std::filesystem::create_directories(Directory);
		std::filesystem::copy(std::filesystem::path(HYP_SOURCE_DIR) / "Content/Shaders", Directory,
		                      std::filesystem::copy_options::recursive |
		                          std::filesystem::copy_options::overwrite_existing);
		FLocalFileSystem Files;
		const std::string_view Triangle = "#include \"Common.hlsli\"\n#include \"Common/ColorSpace.hlsli\"\n"
		                                  R"SHADER(struct FVertexInput
{
	float3 Position : POSITION;
	float4 Color : COLOR0;
	float2 Uv : TEXCOORD0;
};

struct FVertexOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR0;
};

FVertexOutput VSMain(FVertexInput InInput
#if HYP_ENABLE_INSTANCE
                     ,
                     uint InInstanceId : SV_InstanceID
#endif
)
{
	FVertexOutput Output;
#if HYP_ENABLE_INSTANCE
#define HYP_TRANSFORM DrawInstances[InInstanceId].TransformMatrix
#else
#define HYP_TRANSFORM TransformMatrix
#endif
	Output.Position = mul(HYP_TRANSFORM, float4(InInput.Position, 1.0));
	Output.Color = InInput.Color;
	return Output;
}

float4 PSMain(FVertexOutput InInput) : SV_Target0
{
#if HYP_HDR_DISPLAY
	float3 Linear = SrgbToLinear(InInput.Color.rgb);
	return float4(min(Linear / max(1 - Linear, .0001), 65000), InInput.Color.a);
#else
	return InInput.Color;
#endif
})SHADER";
		const std::string_view Shared = "#define HYP_MATERIAL_VIEW_V1\n#define HYP_MATERIAL_OBJECT_V1\n"
		                                "#include \"MaterialBlocks.hlsli\"\n"
		                                R"SHADER(#ifndef ASSET_GAIN
#define ASSET_GAIN 0
#endif

cbuffer Surface : register(b4)
{
	float4 Tint;
};

Texture2D ImageTexture : register(t0);
SamplerState ImageSampler : register(s0);

struct FAssetVertex
{
	float4 Position : SV_Position;
	float2 Uv : TEXCOORD0;
};

FAssetVertex AssetVertex(float3 InPosition : POSITION, float2 InUv : TEXCOORD0)
{
	FAssetVertex Result;
	Result.Position = mul(ViewProjection, mul(World, float4(InPosition, 1)));
	Result.Uv = InUv;
	return Result;
}

float4 AssetPixel(FAssetVertex InInput) : SV_Target0
{
	return float4(Tint.rgb * ImageTexture.Sample(ImageSampler, InInput.Uv).rgb * ASSET_GAIN, Tint.a);
})SHADER";
		Files.WriteAtomic(Directory / "Triangle.hlsl", std::as_bytes(std::span(Triangle)));
		Files.WriteAtomic(Directory / "SharedAsset.hlsl", std::as_bytes(std::span(Shared)));
		return Directory;
	}();
	return Root;
}
} // namespace Hyperion
