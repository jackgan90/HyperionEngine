#include "Common.hlsli"

struct FVertexInput
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

FVertexOutput VSMain(FVertexInput InInput)
{
	FVertexOutput Output;
	Output.Position = mul(TransformMatrix, float4(InInput.Position, 1.0));
	Output.Color = InInput.Color;
	return Output;
}

float4 PSMain(FVertexOutput InInput) : SV_Target0
{
	return InInput.Color;
}
