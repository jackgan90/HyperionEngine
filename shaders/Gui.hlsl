#include "Common.hlsli"
Texture2D FontTexture : register(t0);
SamplerState FontSampler : register(s0);

struct FVertexInput
{
	float2 Position : POSITION;
	float2 Uv : TEXCOORD0;
	float4 Color : COLOR0;
};

struct FVertexOutput
{
	float4 Position : SV_Position;
	float2 Uv : TEXCOORD0;
	float4 Color : COLOR0;
};

FVertexOutput VSMain(FVertexInput InInput)
{
	FVertexOutput Output;
	Output.Position = mul(TransformMatrix, float4(InInput.Position, 0, 1));
	Output.Uv = InInput.Uv;
	Output.Color = InInput.Color;
	return Output;
}

float4 PSMain(FVertexOutput InInput) : SV_Target0
{
	return InInput.Color * FontTexture.Sample(FontSampler, InInput.Uv);
}
