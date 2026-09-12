#define HYP_MATERIAL_VIEW_V1
#define HYP_MATERIAL_OBJECT_V1
#include "MaterialBlocks.hlsli"
#ifndef ASSET_GAIN
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
}
