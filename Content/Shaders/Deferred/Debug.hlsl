#include "DeferredLightingParameters.generated.hlsli"
#include "GBuffer.hlsli"
#if !defined(HYP_GBUFFER_VIEW_LIT) || !defined(HYP_GBUFFER_VIEW_BASE_COLOR) ||                                         \
    !defined(HYP_GBUFFER_VIEW_SHADING_NORMAL) || !defined(HYP_GBUFFER_VIEW_MATERIAL_CHANNELS) ||                       \
    !defined(HYP_GBUFFER_VIEW_EMISSIVE) || !defined(HYP_GBUFFER_VIEW_SCENE_DEPTH) ||                                   \
    !defined(HYP_GBUFFER_VIEW_GEOMETRY_NORMAL)
#error GBuffer visualizer definitions must be supplied by RasterOptions
#endif
Texture2D<float4> GBuffer0 : register(t0);
Texture2D<float4> GBuffer1 : register(t1);
Texture2D<float4> GBuffer2 : register(t2);
Texture2D<float4> GBuffer3 : register(t3);
Texture2D<float> SceneDepth : register(t4);

cbuffer GBufferDebugV1 : register(b0)
{
	FGBufferDebugV1Uniform GBufferDebug;
};

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	int3 Pixel = int3(int2(InPosition.xy), 0);
	float4 Surface = GBuffer2.Load(Pixel);
	if (Surface.z < .5)
	{
		return float4(0, 0, 0, 1);
	}
	float4 Base = GBuffer0.Load(Pixel);
	float4 Normals = GBuffer1.Load(Pixel);
	float3 Color = Base.rgb;
	if (GBufferDebug.Mode == HYP_GBUFFER_VIEW_SHADING_NORMAL)
	{
		Color = DecodeNormal(Normals.xy) * .5 + .5;
	}
	if (GBufferDebug.Mode == HYP_GBUFFER_VIEW_MATERIAL_CHANNELS)
	{
		Color = float3(Surface.x, Base.a, Surface.y);
	}
	if (GBufferDebug.Mode == HYP_GBUFFER_VIEW_EMISSIVE)
	{
		float3 Emissive = max(GBuffer3.Load(Pixel).rgb, 0);
		Color = Emissive / (1 + Emissive);
	}
	if (GBufferDebug.Mode == HYP_GBUFFER_VIEW_SCENE_DEPTH)
	{
		Color = SceneDepth.Load(Pixel).xxx;
	}
	if (GBufferDebug.Mode == HYP_GBUFFER_VIEW_GEOMETRY_NORMAL)
	{
		Color = DecodeNormal(Normals.zw) * .5 + .5;
	}
	return float4(Color, 1);
}
