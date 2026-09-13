#include "../Lighting/DirectLighting.hlsli"
#include "GBuffer.hlsli"

cbuffer LocalVolumeV1 : register(b0)
{
	column_major float4x4 ViewProjection;
	column_major float4x4 World;
};

cbuffer LocalLightV1 : register(b1)
{
	column_major float4x4 InverseViewProjection;
	float4 Viewport;
	float2 DepthRange;
	float3 Eye;
	float3 Position;
	float3 Radiance;
	float3 Direction;
	float4 ConeRange;
};

Texture2D<float4> GBuffer0 : register(t0);
Texture2D<float4> GBuffer1 : register(t1);
Texture2D<float4> GBuffer2 : register(t2);
Texture2D<float> SceneDepth : register(t3);

#include "Reconstruction.hlsli"

float4 VSMain(float3 InPosition : POSITION) : SV_Position
{
	return mul(ViewProjection, mul(World, float4(InPosition, 1)));
}

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	int2 Pixel = int2(InPosition.xy);
	float4 Surface = GBuffer2.Load(int3(Pixel, 0));
	clip(Surface.z - .5);
	float3 Receiver = ReconstructWorld(InPosition.xy, SceneDepth.Load(int3(Pixel, 0)));
	float3 ToLight = Position - Receiver;
	float DistanceSquared = dot(ToLight, ToLight);
	float Distance = sqrt(DistanceSquared);
	float NormalizedDistance = Distance * ConeRange.x;
	clip(1 - NormalizedDistance);
	float3 L = DistanceSquared > 1e-12 ? ToLight * rsqrt(DistanceSquared) : float3(0, 0, 1);
	float Attenuation = saturate(1 - pow(NormalizedDistance, 4));
	Attenuation = Attenuation * Attenuation / max(DistanceSquared, .0001);
	if (ConeRange.w > .5)
	{
		float Angle = dot(Direction, -L);
		clip(Angle - ConeRange.z);
		Attenuation *= smoothstep(ConeRange.z, ConeRange.y, Angle);
	}
	FMaterialParameters Material =
	    DecodeGBuffer(GBuffer0.Load(int3(Pixel, 0)), GBuffer1.Load(int3(Pixel, 0)), Surface, 0);
	float3 Color = EvaluateDirectLighting(Material, Receiver, Eye, L, min(Radiance, 1e20) * Attenuation);
	return float4(min(Color, 65000), 0);
}
