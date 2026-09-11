#include "GBuffer.hlsli"
Texture2D<float4> GBuffer0 : register(t0);
Texture2D<float4> GBuffer1 : register(t1);
Texture2D<float4> GBuffer2 : register(t2);
Texture2D<float4> GBuffer3 : register(t3);
Texture2D<float> SceneDepth : register(t4);

cbuffer GBufferDebugV1 : register(b0)
{
	uint Mode;
};

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	int3 Pixel = int3(int2(InPosition.xy), 0);
	float4 Surface = GBuffer2.Load(Pixel);
	if (Surface.z < .5)
	{
		return float4(0, 0, 0, 1);
	}
	float4 Base = GBuffer0.Load(Pixel);
	float4 Normals = GBuffer1.Load(Pixel);
	float3 Color = Base.rgb;
	if (Mode == 2)
	{
		Color = DecodeNormal(Normals.xy) * .5 + .5;
	}
	if (Mode == 3)
	{
		Color = float3(Surface.x, Base.a, Surface.y);
	}
	if (Mode == 4)
	{
		float3 Emissive = max(GBuffer3.Load(Pixel).rgb, 0);
		Color = Emissive / (1 + Emissive);
	}
	if (Mode == 5)
	{
		Color = SceneDepth.Load(Pixel).xxx;
	}
	if (Mode == 6)
	{
		Color = DecodeNormal(Normals.zw) * .5 + .5;
	}
	return float4(Color, 1);
}
