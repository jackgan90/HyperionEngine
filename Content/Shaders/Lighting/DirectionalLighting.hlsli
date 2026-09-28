#ifndef HYP_DIRECTIONAL_LIGHTING
#define HYP_DIRECTIONAL_LIGHTING

struct FDirectionalLight
{
	float4 Direction;
	float4 Radiance;
};

StructuredBuffer<FDirectionalLight> HyperionDirectionalLightsV1 : register(t3, space3);

float3 EvaluateAdditionalDirectionalLighting(FMaterialParameters InMaterial, float3 InWorld, float3 InEye)
{
	if (InMaterial.bUnlit)
	{
		return 0;
	}
	uint Count;
	uint Stride;
	HyperionDirectionalLightsV1.GetDimensions(Count, Stride);
	float3 Result = 0;
	for (uint Index = 0; Index < Count; ++Index)
	{
		FDirectionalLight Light = HyperionDirectionalLightsV1[Index];
		Result += EvaluateDirectLighting(InMaterial, InWorld, InEye, Light.Direction.xyz, Light.Radiance.xyz);
	}
	return Result;
}
#endif
