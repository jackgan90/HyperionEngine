#include "OutlineParameters.generated.hlsli"
Texture2D<float> OutlineMask : register(t0);

cbuffer OutlineColorV1 : register(b0)
{
	FOutlineColorV1Uniform OutlineColor;
};

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	return float4(OutlineColor.Color.rgb, OutlineColor.Color.a * OutlineMask.Load(int3(int2(InPosition.xy), 0)));
}
