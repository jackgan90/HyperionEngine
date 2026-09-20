Texture2D<float> OutlineMask : register(t0);

cbuffer OutlineColorV1 : register(b0)
{
	float4 Color;
};

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	return float4(Color.rgb, Color.a * OutlineMask.Load(int3(int2(InPosition.xy), 0)));
}
