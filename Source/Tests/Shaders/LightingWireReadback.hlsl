#include "Lighting/ClusteredLighting.hlsli"
#include "Lighting/DirectionalLighting.hlsli"

RWStructuredBuffer<uint> WireOutput : register(u0);

void WriteVector(uint InOffset, float4 InValue)
{
	uint4 Words = asuint(InValue);
	WireOutput[InOffset] = Words.x;
	WireOutput[InOffset + 1] = Words.y;
	WireOutput[InOffset + 2] = Words.z;
	WireOutput[InOffset + 3] = Words.w;
}

[numthreads(1, 1, 1)] void CSMain()
{
	uint LightCount;
	uint LightStride;
	ClusterLights.GetDimensions(LightCount, LightStride);
	uint HeaderCount;
	uint HeaderStride;
	ClusterHeaders.GetDimensions(HeaderCount, HeaderStride);
	uint IndexCount;
	uint IndexStride;
	ClusterIndices.GetDimensions(IndexCount, IndexStride);
	uint DirectionalCount;
	uint DirectionalStride;
	HyperionDirectionalLightsV1.GetDimensions(DirectionalCount, DirectionalStride);
	WireOutput[0] = LightCount;
	WireOutput[1] = LightStride;
	WireOutput[2] = HeaderCount;
	WireOutput[3] = HeaderStride;
	WireOutput[4] = IndexCount;
	WireOutput[5] = IndexStride;
	WireOutput[6] = DirectionalCount;
	WireOutput[7] = DirectionalStride;
	uint Cursor = 8;
	for (uint Index = 0; Index < LightCount; ++Index)
	{
		FClusterLight Light = ClusterLights[Index];
		WriteVector(Cursor, Light.PositionRange);
		WriteVector(Cursor + 4, Light.RadianceType);
		WriteVector(Cursor + 8, Light.DirectionInner);
		WriteVector(Cursor + 12, Light.Outer);
		Cursor += 16;
	}
	for (uint Index = 0; Index < HeaderCount; ++Index)
	{
		uint2 Header = ClusterHeaders[Index];
		WireOutput[Cursor++] = Header.x;
		WireOutput[Cursor++] = Header.y;
	}
	for (uint Index = 0; Index < IndexCount; ++Index)
	{
		WireOutput[Cursor++] = ClusterIndices[Index];
	}
	for (uint Index = 0; Index < DirectionalCount; ++Index)
	{
		FDirectionalLight Light = HyperionDirectionalLightsV1[Index];
		WriteVector(Cursor, Light.Direction);
		WriteVector(Cursor + 4, Light.Radiance);
		Cursor += 8;
	}
}
