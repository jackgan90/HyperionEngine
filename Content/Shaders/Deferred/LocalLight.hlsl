#include "../Lighting/LocalLighting.hlsli"
#include "DeferredLightingParameters.generated.hlsli"
#include "GBuffer.hlsli"

cbuffer LocalVolumeV1 : register(b0)
{
	FLocalVolumeV1Uniform LocalVolume;
};

cbuffer LocalLightV1 : register(b1)
{
	FLocalLightV1Uniform LocalLighting;
};

Texture2D<float4> GBuffer0 : register(t0);
Texture2D<float4> GBuffer1 : register(t1);
Texture2D<float4> GBuffer2 : register(t2);
Texture2D<float> SceneDepth : register(t3);

#include "Reconstruction.hlsli"

float3 ReconstructWorld(float2 InPixel, float InDepth)
{
	return ReconstructWorld(InPixel, InDepth, LocalLighting.Viewport, LocalLighting.DepthRange,
	                        LocalLighting.InverseViewProjection);
}

float4 VSMain(float3 InPosition : POSITION) : SV_Position
{
	return mul(LocalVolume.ViewProjection, mul(LocalVolume.World, float4(InPosition, 1)));
}

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	int2 Pixel = int2(InPosition.xy);
	float4 Surface = GBuffer2.Load(int3(Pixel, 0));
	clip(Surface.z - .5);
	float3 Receiver = ReconstructWorld(InPosition.xy, SceneDepth.Load(int3(Pixel, 0)));
	FMaterialParameters Material =
	    DecodeGBuffer(GBuffer0.Load(int3(Pixel, 0)), GBuffer1.Load(int3(Pixel, 0)), Surface, 0);
	return float4(EvaluateLocalLight(Material, Receiver, LocalLighting.Eye, LocalLighting.Position,
	                                 LocalLighting.Radiance, LocalLighting.Direction, LocalLighting.ConeRange),
	              0);
}
