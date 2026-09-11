#include "Common.hlsli"
#include "Common/ColorSpace.hlsli"
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
	Output.Position = mul(HYP_TRANSFORM, float4(InInput.Position, 0, 1));
	Output.Uv = InInput.Uv;
	Output.Color = InInput.Color;
	return Output;
}

float4 PSMain(FVertexOutput InInput) : SV_Target0
{
	float4 Color = InInput.Color * FontTexture.Sample(FontSampler, InInput.Uv);
#if HYP_GUI_SRGB
	Color.rgb = SrgbToLinear(Color.rgb);
#endif
	return Color;
}
