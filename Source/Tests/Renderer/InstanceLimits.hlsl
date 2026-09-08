#ifndef HYP_ENABLE_INSTANCE
#define HYP_ENABLE_INSTANCE 0
#endif

struct FInstance
{
	float4 Placement;
};

cbuffer InstanceData : register(b0)
{
#if HYP_ENABLE_INSTANCE
	FInstance Records[4];
#else
	float4 Placement;
#endif
};

#if HYP_ENABLE_INSTANCE && HYP_TEST_INSTANCE_CONSTANT_LIMIT
#define HYP_TEST_EXTRA_BLOCK(Index)                                                                                    \
	cbuffer Limit##Index : register(b##Index)                                                                          \
	{                                                                                                                  \
		FInstance Extra##Index[4];                                                                                     \
	};
HYP_TEST_EXTRA_BLOCK(1)
HYP_TEST_EXTRA_BLOCK(2)
HYP_TEST_EXTRA_BLOCK(3)
HYP_TEST_EXTRA_BLOCK(4)
HYP_TEST_EXTRA_BLOCK(5)
HYP_TEST_EXTRA_BLOCK(6)
HYP_TEST_EXTRA_BLOCK(7)
HYP_TEST_EXTRA_BLOCK(8)
HYP_TEST_EXTRA_BLOCK(9)
HYP_TEST_EXTRA_BLOCK(10)
HYP_TEST_EXTRA_BLOCK(11)
HYP_TEST_EXTRA_BLOCK(12)
HYP_TEST_EXTRA_BLOCK(13)
HYP_TEST_EXTRA_BLOCK(14)
#undef HYP_TEST_EXTRA_BLOCK
#endif

struct FOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

FOutput VSMain(float3 InPosition : POSITION
#if HYP_ENABLE_INSTANCE
               ,
               uint InInstanceId : SV_InstanceID
#if HYP_TEST_INSTANCE_INPUT
               ,
               float2 InExtra : TEXCOORD9
#endif
#endif
)
{
#if HYP_ENABLE_INSTANCE
	float4 Position = Records[InInstanceId].Placement;
#else
	float4 Position = Placement;
#endif
	float Extra = 0;
#if HYP_TEST_INSTANCE_CONSTANT_LIMIT
#if HYP_ENABLE_INSTANCE
	Extra = Extra1[InInstanceId].Placement.w + Extra2[InInstanceId].Placement.w + Extra3[InInstanceId].Placement.w +
	        Extra4[InInstanceId].Placement.w + Extra5[InInstanceId].Placement.w + Extra6[InInstanceId].Placement.w +
	        Extra7[InInstanceId].Placement.w + Extra8[InInstanceId].Placement.w + Extra9[InInstanceId].Placement.w +
	        Extra10[InInstanceId].Placement.w + Extra11[InInstanceId].Placement.w + Extra12[InInstanceId].Placement.w +
	        Extra13[InInstanceId].Placement.w + Extra14[InInstanceId].Placement.w;
#else
	Extra = Placement.w * 14;
#endif
#endif
	FOutput Result;
	Result.Position = float4(InPosition.xy * Position.w + Position.xy, InPosition.z + Position.z, 1);
#if HYP_ENABLE_INSTANCE && HYP_TEST_INSTANCE_INPUT
	Result.Position.xy += InExtra;
#endif
	Result.Color = float4(.2 + Extra * .01, .6, .8, 1);
	return Result;
}

float4 PSMain(FOutput InInput) : SV_Target0
{
	return InInput.Color;
}
