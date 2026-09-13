#ifndef HYP_ENVIRONMENT_LIGHTING
#define HYP_ENVIRONMENT_LIGHTING

cbuffer EnvironmentV1 : register(b0, space2)
{
	float4 EnvironmentControl;
	float4 EnvironmentRotation;
	float4 EnvironmentSh0;
	float4 EnvironmentSh1;
	float4 EnvironmentSh2;
	float4 EnvironmentSh3;
	float4 EnvironmentSh4;
	float4 EnvironmentSh5;
	float4 EnvironmentSh6;
	float4 EnvironmentSh7;
	float4 EnvironmentSh8;
};

TextureCube<float4> EnvironmentSpecular : register(t0, space2);
Texture2D<float4> EnvironmentBrdf : register(t1, space2);
SamplerState EnvironmentSampler : register(s0, space2);

float3 EnvironmentDirection(float3 InDirection)
{
	return float3(EnvironmentRotation.x * InDirection.x - EnvironmentRotation.y * InDirection.z, InDirection.y,
	              EnvironmentRotation.y * InDirection.x + EnvironmentRotation.x * InDirection.z);
}

float3 EnvironmentIrradiance(float3 InNormal)
{
	float3 N = EnvironmentDirection(InNormal);
	float3 E = EnvironmentSh0.xyz * .282094792 + EnvironmentSh1.xyz * (.488602512 * N.y) +
	           EnvironmentSh2.xyz * (.488602512 * N.z) + EnvironmentSh3.xyz * (.488602512 * N.x) +
	           EnvironmentSh4.xyz * (1.092548431 * N.x * N.y) + EnvironmentSh5.xyz * (1.092548431 * N.y * N.z) +
	           EnvironmentSh6.xyz * (.315391565 * (3 * N.z * N.z - 1)) +
	           EnvironmentSh7.xyz * (1.092548431 * N.x * N.z) +
	           EnvironmentSh8.xyz * (.546274215 * (N.x * N.x - N.y * N.y));
	return max(E, 0);
}

float3 EvaluateEnvironmentLighting(FMaterialParameters InMaterial, float3 InView)
{
	float NoV = saturate(dot(InMaterial.Normal, InView));
	float3 F0 = lerp(float3(.04, .04, .04), InMaterial.BaseColor, InMaterial.Metallic);
	float3 Fresnel = F0 + (max(1 - InMaterial.Roughness, F0) - F0) * pow(1 - NoV, 5);
	float3 Diffuse = (1 - Fresnel) * (1 - InMaterial.Metallic) * InMaterial.BaseColor *
	                 EnvironmentIrradiance(InMaterial.Normal) / 3.14159265;
	float3 R = EnvironmentDirection(reflect(-InView, InMaterial.Normal));
	float3 Prefiltered =
	    EnvironmentSpecular.SampleLevel(EnvironmentSampler, R, InMaterial.Roughness * EnvironmentControl.z).rgb;
	float2 Brdf = EnvironmentBrdf.SampleLevel(EnvironmentSampler, float2(NoV, InMaterial.Roughness), 0).rg;
	float3 Specular = Prefiltered * (F0 * Brdf.x + Brdf.y);
	return (Diffuse + Specular) * EnvironmentControl.y * InMaterial.Occlusion;
}
#endif
