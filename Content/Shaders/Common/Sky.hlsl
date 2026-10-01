#include "SkyParameters.generated.hlsli"

cbuffer SkyViewV1 : register(b0)
{
	FSkyViewV1Uniform Sky;
};

TextureCube<float4> SkyRadiance : register(t0);
SamplerState SkySampler : register(s0);

float4 VSMain(float2 InPosition : POSITION) : SV_Position
{
#if HYP_REVERSED_SKY
	return float4(InPosition, 0, 1);
#else
	return float4(InPosition, 1, 1);
#endif
}

[earlydepthstencil] float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	float2 Ndc = (InPosition.xy - Sky.SkyViewport.xy) / Sky.SkyViewport.zw * float2(2, -2) + float2(-1, 1);
	float3 D = normalize(mul(Sky.InverseSkyViewProjection, float4(Ndc, .5, 1)).xyz);
	float3 R = float3(Sky.SkyRotationIntensity.x * D.x - Sky.SkyRotationIntensity.y * D.z, D.y,
	                  Sky.SkyRotationIntensity.y * D.x + Sky.SkyRotationIntensity.x * D.z);
	return float4(SkyRadiance.Sample(SkySampler, R).rgb * Sky.SkyRotationIntensity.z * Sky.SkyTint.rgb, 1);
}
