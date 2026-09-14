#ifndef HYP_ENABLE_INSTANCE
#define HYP_ENABLE_INSTANCE 0
#endif
#if HYP_ENABLE_INSTANCE
#ifndef HYP_INSTANCE_CAPACITY
#define HYP_INSTANCE_CAPACITY 128
#endif
struct FDrawInstance
{
	column_major float4x4 TransformMatrix;
};

cbuffer DrawConstants : register(b0)
{
	FDrawInstance DrawInstances[HYP_INSTANCE_CAPACITY];
};
#else
cbuffer DrawConstants : register(b0)
{
	column_major float4x4 TransformMatrix;
};
#endif
