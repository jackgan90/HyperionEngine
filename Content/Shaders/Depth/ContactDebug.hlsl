Texture2D<float> Source : register(t0);

cbuffer PreviewV1 : register(b0)
{
	float4 Viewport;
	uint Mip;
	uint bInvert;
};

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	uint Width;
	uint Height;
	uint Levels;
	Source.GetDimensions(Mip, Width, Height, Levels);
	float2 Uv = (InPosition.xy - Viewport.xy) / Viewport.zw;
	int2 Pixel = int2(clamp(Uv * float2(Width, Height), 0, float2(Width, Height) - 1));
	float Value = Source.Load(int3(Pixel, Mip));
	Value = bInvert ? 1 - Value : Value;
	return float4(Value, Value, Value, 1);
}
