#include "../Lighting/SurfaceLighting.hlsli"
#include "GBuffer.hlsli"
#if HYP_CLUSTERED
#include "../Lighting/ClusteredLighting.hlsli"
#endif
Texture2D<float4> GBuffer0 : register(t0);
Texture2D<float4> GBuffer1 : register(t1);
Texture2D<float4> GBuffer2 : register(t2);
Texture2D<float4> GBuffer3 : register(t3);
Texture2D<float> SceneDepth : register(t4);

cbuffer DeferredLightV1 : register(b0)
{
	column_major float4x4 InverseViewProjection;
	float4 Viewport;
	float2 DepthRange;
	float3 Eye;
#if !HYP_NO_DIRECTIONAL
	float3 LightDirection;
	float3 LightColor;
#endif
	float3 Ambient;
};

#include "Reconstruction.hlsli"

float3 NeighborDelta(int2 InPixel, int2 InOffset, float3 InWorld, float3 InNormal)
{
	int2 P = InPixel + InOffset;
	if (any(P < int2(Viewport.xy)) || any(P >= int2(Viewport.xy + Viewport.zw)))
	{
		return 0;
	}
	float Depth = SceneDepth.Load(int3(P, 0));
	float4 Surface = GBuffer2.Load(int3(P, 0));
	float3 Normal = DecodeNormal(GBuffer1.Load(int3(P, 0)).zw);
	float3 Delta = ReconstructWorld(float2(P) + .5, Depth) - InWorld;
	// Avoid receiver derivatives across uncovered pixels, silhouettes and depth discontinuities.
	float Limit = max(length(InWorld - Eye) * .02, .001);
	if (Surface.z < .5 || dot(Normal, InNormal) < .8 || length(Delta) > Limit)
	{
		return 0;
	}
	return Delta;
}

float3 ReceiverDerivative(int2 InPixel, int2 InOffset, float3 InWorld, float3 InNormal)
{
	float3 Positive = NeighborDelta(InPixel, InOffset, InWorld, InNormal);
	float3 Negative = -NeighborDelta(InPixel, -InOffset, InWorld, InNormal);
	float A = dot(Positive, Positive);
	float B = dot(Negative, Negative);
	return A > 0 && (B == 0 || A < B) ? Positive : Negative;
}

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	int2 Pixel = int2(InPosition.xy);
	float4 Surface = GBuffer2.Load(int3(Pixel, 0));
	clip(Surface.z - .5);
	FMaterialParameters Material = DecodeGBuffer(GBuffer0.Load(int3(Pixel, 0)), GBuffer1.Load(int3(Pixel, 0)), Surface,
	                                             GBuffer3.Load(int3(Pixel, 0)));
	float3 World = ReconstructWorld(InPosition.xy, SceneDepth.Load(int3(Pixel, 0)));
#if HYP_NO_DIRECTIONAL
	float3 Color = EvaluateIndirectLighting(Material, Ambient);
#else
	float3 Dx = ReceiverDerivative(Pixel, int2(1, 0), World, Material.GeometricNormal);
	float3 Dy = ReceiverDerivative(Pixel, int2(0, 1), World, Material.GeometricNormal);
	float3 Color = EvaluateLighting(Material, World, Eye, LightDirection, LightColor, Ambient, Dx, Dy);
#endif
#if HYP_CLUSTERED
	Color += EvaluateClusteredLighting(Material, World, Eye, InPosition.xy);
#endif
	return float4(Color, 1);
}
