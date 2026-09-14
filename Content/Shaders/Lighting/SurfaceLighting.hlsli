#include "../CascadedShadows.hlsli"
#include "DirectLighting.hlsli"
#include "EnvironmentLighting.hlsli"

float3 EvaluateIndirectLighting(FMaterialParameters InMaterial, float3 InAmbient, float3 InView)
{
	if (EnvironmentControl.x > .5)
	{
		return EvaluateEnvironmentLighting(InMaterial, InView) + InMaterial.Emissive;
	}
	float3 F0 = lerp(float3(.04, .04, .04), InMaterial.BaseColor, InMaterial.Metallic);
	return (InMaterial.BaseColor * (1 - InMaterial.Metallic) + F0 * (.7 - .4 * InMaterial.Roughness)) * InAmbient *
	           InMaterial.Occlusion +
	       InMaterial.Emissive;
}

float3 EvaluateLighting(FMaterialParameters InMaterial, float3 InWorld, float3 InCamera, float3 InLight,
                        float3 InLightColor, float3 InAmbient, float3 InDx, float3 InDy, float InContactVisibility)
{
	if (InMaterial.bUnlit)
	{
		return InMaterial.BaseColor;
	}
	float3 Ambient = EvaluateIndirectLighting(InMaterial, InAmbient, normalize(InCamera - InWorld));
	float Shadow =
	    min(InContactVisibility, DirectionalShadow(InWorld, InMaterial.GeometricNormal, InLight, InDx, InDy));
	float3 Color = EvaluateDirectLighting(InMaterial, InWorld, InCamera, InLight, InLightColor) * Shadow + Ambient;
	return ShadowDebugColor(Color, InWorld);
}

float3 EvaluateLighting(FMaterialParameters InMaterial, float3 InWorld, float3 InCamera, float3 InLight,
                        float3 InLightColor, float3 InAmbient, float3 InDx, float3 InDy)
{
	return EvaluateLighting(InMaterial, InWorld, InCamera, InLight, InLightColor, InAmbient, InDx, InDy, 1);
}
