float3 ReconstructWorld(float2 InPixel, float InDepth)
{
	float2 Ndc = ((InPixel - Viewport.xy) / Viewport.zw) * float2(2, -2) + float2(-1, 1);
	float NdcDepth = (InDepth - DepthRange.x) * DepthRange.y;
	float4 World = mul(InverseViewProjection, float4(Ndc, NdcDepth, 1));
	return World.xyz / World.w;
}
