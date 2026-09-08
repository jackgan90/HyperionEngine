#ifndef HYP_ENABLE_INSTANCE
#define HYP_ENABLE_INSTANCE 0
#endif
#if FIXED_ZERO
#undef HYP_ENABLE_INSTANCE
#define HYP_ENABLE_INSTANCE 0
#endif
struct FPayload
{
	float3 Color;
	uint Mode;
	bool bVisible;
	int Sign;
	float2 Pair[2];
	row_major float2x3 Matrix;
};

struct FInstance
{
	float4 Placement;
	FPayload Payload;
};

cbuffer InstanceData : register(b1)
{
#if HYP_ENABLE_INSTANCE
	FInstance Records[4];
#else
	float4 Placement;
	FPayload Payload;
#endif
};

cbuffer SharedData : register(b2)
{
	float4 SharedTint;
};

Texture2D Maps[2] : register(t0);
SamplerState MapSampler : register(s0);
StructuredBuffer<float4> ReadData : register(t2);

struct FOutput
{
	float4 Position : SV_Position;
#if HYP_ENABLE_INSTANCE
	nointerpolation uint InstanceId : TEXCOORD0;
#endif
};
FOutput VSMain(float3 InPosition : POSITION
#if MISSING_INPUT
               ,
               float2 InExtra : TEXCOORD9
#endif
#if HYP_ENABLE_INSTANCE
               ,
               uint InInstanceId : SV_InstanceID
#endif
)
{
	FOutput Result;
#if MISSING_INPUT
	InPosition.xy += InExtra;
#endif
#if HYP_ENABLE_INSTANCE
	Result.InstanceId = InInstanceId;
	float4 Position = Records[InInstanceId].Placement;
	FPayload Value = Records[InInstanceId].Payload;
#else
	float4 Position = Placement;
	FPayload Value = Payload;
#endif
	float2 Offset = float2(Value.Matrix[0][2], Value.Matrix[1][0]) * .001;
	Result.Position = float4(InPosition.xy * Position.w + Position.xy + Offset, InPosition.z + Position.z, 1);
	return Result;
}

float4 PSMain(FOutput InInput) : SV_Target0
{
#if HYP_ENABLE_INSTANCE
	FPayload Value = Records[InInput.InstanceId].Payload;
#else
	FPayload Value = Payload;
#endif
	float3 Color = Value.Color + float3(Value.Pair[0].y, Value.Pair[1].x, Value.Matrix[1][2]) * .01;
	Color += (float(Value.Mode) + Value.Sign) * .01;
	float4 TextureColor = Maps[0].Sample(MapSampler, float2(.5, .5)) * Maps[1].Sample(MapSampler, float2(.5, .5));
	return float4(Color * (Value.bVisible ? 1 : 0), 1) * SharedTint * TextureColor * ReadData[0];
}
