cbuffer DepthParameters : register(b0)
{
	uint Width;
	uint Height;
	uint SourceWidth;
	uint SourceHeight;
	uint bMaximum;
};

Texture2D<float> SourceDepth : register(t0);
RWTexture2D<float> OutputDepth : register(u0);

[numthreads(8, 8, 1)] void CSMain(uint3 InId : SV_DispatchThreadID)
{
	if (InId.x >= Width || InId.y >= Height)
	{
		return;
	}
	uint2 SourceSize = uint2(SourceWidth, SourceHeight);
	uint2 DestinationSize = uint2(Width, Height);
	// Conservative normalized footprints include odd edges and overlapping boundary texels.
	uint2 First = InId.xy * SourceSize / DestinationSize;
	uint2 End = min(((InId.xy + 1) * SourceSize + DestinationSize - 1) / DestinationSize, SourceSize);
	float Depth = bMaximum ? 0 : 1;
	for (uint Y = First.y; Y < End.y; ++Y)
	{
		for (uint X = First.x; X < End.x; ++X)
		{
			float Sample = SourceDepth.Load(int3(X, Y, 0));
			Depth = bMaximum ? max(Depth, Sample) : min(Depth, Sample);
		}
	}
	OutputDepth[InId.xy] = Depth;
}
