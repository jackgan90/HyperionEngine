cbuffer DepthParameters : register(b0)
{
	uint Width;
	uint Height;
	float2 DepthRange;
	float4 Viewport;
	float FarDepth;
};

Texture2D<float> SourceDepth : register(t0);
RWTexture2D<float> OutputDepth : register(u0);

[numthreads(8, 8, 1)] void CSMain(uint3 InId : SV_DispatchThreadID)
{
	if (InId.x >= Width || InId.y >= Height)
	{
		return;
	}
	float2 Pixel = float2(InId.xy) + .5;
	bool bCovered = all(Pixel >= Viewport.xy) && all(Pixel < Viewport.xy + Viewport.zw);
	OutputDepth[InId.xy] =
	    bCovered ? saturate((SourceDepth.Load(int3(InId.xy, 0)) - DepthRange.x) * DepthRange.y) : FarDepth;
}
