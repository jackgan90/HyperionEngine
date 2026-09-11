#ifndef HYP_MATERIAL_PARAMETERS
#define HYP_MATERIAL_PARAMETERS

struct FMaterialParameters
{
	float3 BaseColor;
	float3 Normal;
	float3 GeometricNormal;
	float3 Emissive;
	float Metallic;
	float Roughness;
	float Occlusion;
	float Alpha;
	bool bUnlit;
};
#endif
