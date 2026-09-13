#ifndef HYP_LOCAL_LIGHTING
#define HYP_LOCAL_LIGHTING
#include "DirectLighting.hlsli"

float3 EvaluateLocalLight(FMaterialParameters InMaterial, float3 InWorld, float3 InEye, float3 InPosition,
                          float3 InRadiance, float3 InDirection, float4 InConeRange)
{
	float3 ToLight = InPosition - InWorld;
	float DistanceSquared = dot(ToLight, ToLight);
	float NormalizedDistance = sqrt(DistanceSquared) * InConeRange.x;
	if (NormalizedDistance >= 1)
	{
		return 0;
	}
	float3 L = DistanceSquared > 1e-12 ? ToLight * rsqrt(DistanceSquared) : float3(0, 0, 1);
	float Attenuation = saturate(1 - pow(NormalizedDistance, 4));
	Attenuation = Attenuation * Attenuation / max(DistanceSquared, .0001);
	if (InConeRange.w > .5)
	{
		float Angle = dot(InDirection, -L);
		if (Angle <= InConeRange.z)
		{
			return 0;
		}
		Attenuation *= smoothstep(InConeRange.z, InConeRange.y, Angle);
	}
	return min(EvaluateDirectLighting(InMaterial, InWorld, InEye, L, min(InRadiance, 1e20) * Attenuation), 65000);
}
#endif
