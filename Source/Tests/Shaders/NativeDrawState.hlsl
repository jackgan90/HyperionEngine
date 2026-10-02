cbuffer DrawTint : register(b0)
{
	float4 Tint;
};

Texture2D<float4> Image : register(t0);
SamplerState PointSampler : register(s0);

float4 VSMain(float2 InPosition : POSITION) : SV_Position
{
	return float4(InPosition, .5, 1);
}

float4 PSMain() : SV_Target0
{
	return Tint * Image.SampleLevel(PointSampler, float2(.5, .5), 0);
}
