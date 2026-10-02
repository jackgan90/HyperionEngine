#ifndef HYP_LOCAL_LIGHTING
#define HYP_LOCAL_LIGHTING
#include "DirectLighting.hlsli"

struct FLocalLightAttenuation
{
	float InverseRange;
	float InnerCos;
	float OuterCos;
	float SpotFlag;
};

FLocalLightAttenuation DecodeLocalLightAttenuation(float4 InConeRange)
{
	FLocalLightAttenuation Result;
	Result.InverseRange = InConeRange.x;
	Result.InnerCos = InConeRange.y;
	Result.OuterCos = InConeRange.z;
	Result.SpotFlag = InConeRange.w;
	return Result;
}

float3 EvaluateLocalLight(FMaterialParameters InMaterial, float3 InWorld, float3 InEye, float3 InPosition,
                          float3 InRadiance, float3 InDirection, FLocalLightAttenuation InAttenuation)
{
	float3 ToLight = InPosition - InWorld;
	float DistanceSquared = dot(ToLight, ToLight);
	float NormalizedDistance = sqrt(DistanceSquared) * InAttenuation.InverseRange;
	if (NormalizedDistance >= 1)
	{
		return 0;
	}
	float3 L = DistanceSquared > 1e-12 ? ToLight * rsqrt(DistanceSquared) : float3(0, 0, 1);
	float Attenuation = saturate(1 - pow(NormalizedDistance, 4));
	Attenuation = Attenuation * Attenuation / max(DistanceSquared, .0001);
	if (InAttenuation.SpotFlag > .5)
	{
		float Angle = dot(InDirection, -L);
		if (Angle <= InAttenuation.OuterCos)
		{
			return 0;
		}
		Attenuation *= smoothstep(InAttenuation.OuterCos, InAttenuation.InnerCos, Angle);
	}
	return min(EvaluateDirectLighting(InMaterial, InWorld, InEye, L, min(InRadiance, 1e20) * Attenuation), 65000);
}

float3 EvaluateLocalLight(FMaterialParameters InMaterial, float3 InWorld, float3 InEye, float3 InPosition,
                          float3 InRadiance, float3 InDirection, float4 InConeRange)
{
	return EvaluateLocalLight(InMaterial, InWorld, InEye, InPosition, InRadiance, InDirection,
	                          DecodeLocalLightAttenuation(InConeRange));
}
#endif
