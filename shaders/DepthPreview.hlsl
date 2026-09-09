Texture2D<float> DepthMap : register(t0);

struct FPreviewVertex
{
	float4 Position : SV_Position;
	float2 Uv : TEXCOORD0;
};

FPreviewVertex VSMain(float2 InPosition : POSITION)
{
	FPreviewVertex Result;
	Result.Position = float4(InPosition, 0, 1);
	Result.Uv = InPosition * float2(.5, -.5) + .5;
	return Result;
}

float4 PSMain(FPreviewVertex InVertex) : SV_Target0
{
	uint Width;
	uint Height;
	DepthMap.GetDimensions(Width, Height);
	int2 Pixel = min(int2(saturate(InVertex.Uv) * float2(Width, Height)), int2(Width, Height) - 1);
	float Depth = DepthMap.Load(int3(Pixel, 0));
	return float4(Depth, Depth, Depth, 1);
}
