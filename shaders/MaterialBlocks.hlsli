#ifndef HYP_MATERIAL_BLOCKS_V1
#define HYP_MATERIAL_BLOCKS_V1
#ifndef HYP_ENABLE_INSTANCE
#define HYP_ENABLE_INSTANCE 0
#endif
// Opt in before inclusion. These block names and offsets identify ABI version 1.
#if defined(HYP_MATERIAL_VIEW_V1)
cbuffer HyperionViewV1 : register(b0, space0)
{
	column_major float4x4 ViewProjection;
	float3 CameraPosition;
	float ViewPadding;
};
#endif
#if defined(HYP_MATERIAL_OBJECT_V1)
#if HYP_ENABLE_INSTANCE
struct FObjectInstance
{
	column_major float4x4 World;
	column_major float4x4 Normal;
	float OrientationSign;
};

cbuffer HyperionObjectV1 : register(b1, space0)
{
	FObjectInstance ObjectInstances[128];
};
#else
cbuffer HyperionObjectV1 : register(b1, space0)
{
	column_major float4x4 World;
	column_major float4x4 Normal;
	float OrientationSign;
	float3 ObjectPadding;
};
#endif
#endif
#if defined(HYP_MATERIAL_SURFACE_V1)
#if HYP_ENABLE_INSTANCE
struct FSurfaceInstance
{
	float4 BaseColor;
	float3 Emissive;
	float NormalScale;
	float Metallic;
	float Roughness;
	float OcclusionStrength;
	float AlphaCutoff;
	uint BaseColorUv;
	uint MetallicRoughnessUv;
	uint NormalUv;
	uint OcclusionUv;
	uint EmissiveUv;
	uint AlphaMode;
	bool bDoubleSided;
	bool bUnlit;
	bool bHasNormal;
};

cbuffer HyperionMaterialV1 : register(b2, space0)
{
	FSurfaceInstance SurfaceInstances[128];
};
#else
cbuffer HyperionMaterialV1 : register(b2, space0)
{
	float4 BaseColor;
	float3 Emissive;
	float NormalScale;
	float Metallic;
	float Roughness;
	float OcclusionStrength;
	float AlphaCutoff;
	uint BaseColorUv;
	uint MetallicRoughnessUv;
	uint NormalUv;
	uint OcclusionUv;
	uint EmissiveUv;
	uint AlphaMode;
	bool bDoubleSided;
	bool bUnlit;
	bool bHasNormal;
	float3 MaterialPadding;
};
#endif
#endif
#if defined(HYP_MATERIAL_SCENE_V1)
cbuffer HyperionSceneV1 : register(b3, space0)
{
	float3 MainLightDirection;
	float SceneDirectionPadding;
	float3 MainLightColor;
	float SceneColorPadding;
	float3 AmbientColor;
	float SceneAmbientPadding;
};
#endif
#endif
