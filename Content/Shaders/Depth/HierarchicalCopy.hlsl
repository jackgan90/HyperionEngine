#include "HierarchicalDepthParameters.generated.hlsli"

cbuffer HZBCopyV1 : register(b0)
{
	FHZBCopyV1Uniform HierarchicalCopy;
};

Texture2D<float> SourceDepth : register(t0);
RWTexture2D<float> OutputDepth : register(u0);

[numthreads(8, 8, 1)] void CSMain(uint3 InId : SV_DispatchThreadID)
{
	if (InId.x >= HierarchicalCopy.Width || InId.y >= HierarchicalCopy.Height)
	{
		return;
	}
	float2 Pixel = float2(InId.xy) + .5;
	bool bCovered = all(Pixel >= HierarchicalCopy.Viewport.xy) &&
	                all(Pixel < HierarchicalCopy.Viewport.xy + HierarchicalCopy.Viewport.zw);
	OutputDepth[InId.xy] = bCovered ? saturate((SourceDepth.Load(int3(InId.xy, 0)) - HierarchicalCopy.DepthRange.x) *
	                                           HierarchicalCopy.DepthRange.y)
	                                : HierarchicalCopy.FarDepth;
}
