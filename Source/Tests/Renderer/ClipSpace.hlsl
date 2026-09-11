struct FClipInstance
{
	column_major float4x4 Transform;
	float4 Tint;
	float Orientation;
};

cbuffer ClipData : register(b0)
{
#if HYP_ENABLE_INSTANCE
	FClipInstance Instances[8];
#else
	column_major float4x4 Transform;
	float4 Tint;
	float Orientation;
#endif
};

struct FOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR0;
};
FOutput VSMain(float3 InPosition : POSITION
#if HYP_ENABLE_INSTANCE
               ,
               uint InInstanceId : SV_InstanceID
#endif
)
{
#if HYP_ENABLE_INSTANCE
#define HYP_CLIP_VALUE(Name) Instances[InInstanceId].Name
#else
#define HYP_CLIP_VALUE(Name) Name
#endif
	FOutput Result;
	Result.Position = mul(HYP_CLIP_VALUE(Transform), float4(InPosition, 1));
	Result.Color = HYP_CLIP_VALUE(Tint);
	Result.Color.g *= HYP_CLIP_VALUE(Orientation) < 0 ? .5 : 1;
	return Result;
}

float4 PSMain(FOutput InInput, bool bInFront : SV_IsFrontFace) : SV_Target0
{
	return bInFront ? InInput.Color : float4(1, 0, 1, 1);
}
