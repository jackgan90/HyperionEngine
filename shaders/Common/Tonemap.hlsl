Texture2D<float4> SceneColor : register(t0);

cbuffer OutputV1 : register(b0)
{
	float Exposure;
};

float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	float3 Hdr = max(SceneColor.Load(int3(int2(InPosition.xy), 0)).rgb * Exposure, 0);
	// Return display-linear color. The sRGB RTV performs the sole transfer encoding.
	return float4(Hdr / (1 + Hdr), 1);
}
