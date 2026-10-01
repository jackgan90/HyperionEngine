#include "ObjectParameters.generated.hlsli"
#include "PbrParameters.generated.hlsli"
#include "SceneLightingParameters.generated.hlsli"
#ifndef HYP_MATERIAL_BLOCKS_V1
#define HYP_MATERIAL_BLOCKS_V1
#include "HyperionUniforms.generated.hlsli"
#ifndef HYP_ENABLE_INSTANCE
#define HYP_ENABLE_INSTANCE 0
#endif
// Opt in before inclusion. These block names and offsets identify ABI version 1.
#if defined(HYP_MATERIAL_VIEW_V1)
HYP_UNIFORM_HyperionViewV1(b0);
#endif
#if defined(HYP_MATERIAL_OBJECT_V1)
#if HYP_ENABLE_INSTANCE
struct FObjectInstance
{
	HYP_RECORD_HyperionObjectV1
};

cbuffer HyperionObjectV1 : register(b1)
{
	FObjectInstance ObjectInstances[128];
};
#else
HYP_UNIFORM_HyperionObjectV1(b1);
#endif
#endif
#if defined(HYP_MATERIAL_SURFACE_V1)
#if HYP_ENABLE_INSTANCE
struct FSurfaceInstance
{
	HYP_RECORD_HyperionMaterialV1
};

cbuffer HyperionMaterialV1 : register(b2)
{
	FSurfaceInstance SurfaceInstances[128];
};
#else
HYP_UNIFORM_HyperionMaterialV1(b2);
#endif
#endif
#if defined(HYP_MATERIAL_SCENE_V1)
HYP_UNIFORM_HyperionSceneV1(b3);
#endif
#endif
