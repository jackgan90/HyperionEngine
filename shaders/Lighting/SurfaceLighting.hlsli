#include "../CascadedShadows.hlsli"
#include "DirectLighting.hlsli"

float3 EvaluateLighting(FMaterialParameters InMaterial, float3 InWorld, float3 InCamera, float3 InLight,
                        float3 InLightColor, float3 InAmbient, float3 InDx, float3 InDy)
{
	if (InMaterial.bUnlit)
	{
		return InMaterial.BaseColor;
	}
	float3 F0 = lerp(float3(.04, .04, .04), InMaterial.BaseColor, InMaterial.Metallic);
	float3 Ambient = (InMaterial.BaseColor * (1 - InMaterial.Metallic) + F0 * (.7 - .4 * InMaterial.Roughness)) *
	                 InAmbient * InMaterial.Occlusion;
	float Shadow = DirectionalShadow(InWorld, InMaterial.GeometricNormal, InLight, InDx, InDy);
	float3 Color = EvaluateDirectLighting(InMaterial, InWorld, InCamera, InLight, InLightColor) * Shadow + Ambient +
	               InMaterial.Emissive;
	return ShadowDebugColor(Color, InWorld);
}
