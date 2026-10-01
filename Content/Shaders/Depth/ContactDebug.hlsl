#include "DepthPreviewParameters.generated.hlsli"
Texture2D<float> HyperionPreviewSource : register(t0);

cbuffer PreviewV1 : register(b0)
{
	FPreviewV1Uniform Preview;
};

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	uint Width;
	uint Height;
	uint Levels;
	HyperionPreviewSource.GetDimensions(Preview.Mip, Width, Height, Levels);
	float2 Uv = (InPosition.xy - Preview.Viewport.xy) / Preview.Viewport.zw;
	int2 Pixel = int2(clamp(Uv * float2(Width, Height), 0, float2(Width, Height) - 1));
	float Value = HyperionPreviewSource.Load(int3(Pixel, Preview.Mip));
	Value = Preview.bInvert ? 1 - Value : Value;
	return float4(Value, Value, Value, 1);
}
