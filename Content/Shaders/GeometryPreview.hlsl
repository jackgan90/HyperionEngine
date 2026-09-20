#define HYP_MATERIAL_VIEW_V1
#define HYP_MATERIAL_OBJECT_V1
#include "MaterialBlocks.hlsli"

struct FVertexInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct FVertexOutput
{
	float4 Position : SV_Position;
	float3 Normal : TEXCOORD0;
};

FVertexOutput VSMain(FVertexInput InInput)
{
	FVertexOutput Output;
	Output.Position = mul(ViewProjection, mul(World, float4(InInput.Position, 1)));
	Output.Normal = mul((float3x3)Normal, InInput.Normal);
	return Output;
}

float4 PSMain(FVertexOutput InInput) : SV_Target
{
	float Light = 0.22 + 0.58 * abs(dot(normalize(InInput.Normal), normalize(float3(0.4, 0.8, 0.5))));
	return float4(float3(0.55, 0.7, 0.82) * Light, 1);
}
