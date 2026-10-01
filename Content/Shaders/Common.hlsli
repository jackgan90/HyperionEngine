#include "HyperionUniforms.generated.hlsli"
#include "ObjectParameters.generated.hlsli"
#ifndef HYP_ENABLE_INSTANCE
#define HYP_ENABLE_INSTANCE 0
#endif
#if HYP_ENABLE_INSTANCE
#ifndef HYP_INSTANCE_CAPACITY
#define HYP_INSTANCE_CAPACITY 128
#endif
struct FDrawInstance
{
	HYP_RECORD_DrawConstants
};

cbuffer DrawConstants : register(b0)
{
	FDrawInstance DrawInstances[HYP_INSTANCE_CAPACITY];
};
#else
HYP_UNIFORM_DrawConstants(b0);
#endif
