#include "Common.hlsli"
#include "Common/ColorSpace.hlsli"

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
}
