#ifndef HYP_CLUSTERED_LIGHTING
#define HYP_CLUSTERED_LIGHTING
#include "LocalLighting.hlsli"

struct FClusterLight
{
	float4 PositionRange;
	float4 RadianceType;
	float4 DirectionInner;
	float4 Outer;
};

cbuffer ClusterViewV1 : register(b0, space3)
{
	float4 ClusterViewport;
	float4 ClusterGrid;
	float4 ClusterCamera;
	float4 ClusterForward;
	float4 ClusterDepth;
};

StructuredBuffer<FClusterLight> ClusterLights : register(t0, space3);
StructuredBuffer<uint2> ClusterHeaders : register(t1, space3);
StructuredBuffer<uint> ClusterIndices : register(t2, space3);

float3 EvaluateClusteredLighting(FMaterialParameters InMaterial, float3 InWorld, float3 InEye, float2 InPixel)
{
	if (ClusterGrid.w < .5 || InMaterial.bUnlit)
	{
		return 0;
	}
	float Depth = dot(InWorld - ClusterCamera.xyz, ClusterForward.xyz);
	float2 Tile = floor((InPixel - ClusterViewport.xy) * ClusterViewport.zw);
	if (Depth <= 0 || any(Tile < 0) || any(Tile >= ClusterGrid.xy))
	{
		return 0;
	}
	uint3 Cell;
	Cell.xy = uint2(Tile);
	Cell.z =
	    uint(clamp(floor(log2(max(Depth, ClusterDepth.z)) * ClusterDepth.x + ClusterDepth.y), 0, ClusterGrid.z - 1));
	uint Index = Cell.x + uint(ClusterGrid.x) * (Cell.y + uint(ClusterGrid.y) * Cell.z);
	uint2 Header = ClusterHeaders[Index];
	float3 Color = 0;
	for (uint Light = 0; Light < Header.y; ++Light)
	{
		FClusterLight Data = ClusterLights[ClusterIndices[Header.x + Light]];
		Color += EvaluateLocalLight(
		    InMaterial, InWorld, InEye, Data.PositionRange.xyz, Data.RadianceType.xyz, Data.DirectionInner.xyz,
		    float4(Data.PositionRange.w, Data.DirectionInner.w, Data.Outer.x, Data.RadianceType.w));
	}
	return Color;
}
#endif
