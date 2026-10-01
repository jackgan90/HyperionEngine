float3 ReconstructWorld(float2 InPixel, float InDepth, float4 InViewport, float2 InDepthRange,
                        float4x4 InInverseViewProjection)
{
	float2 Ndc = ((InPixel - InViewport.xy) / InViewport.zw) * float2(2, -2) + float2(-1, 1);
	float NdcDepth = (InDepth - InDepthRange.x) * InDepthRange.y;
	float4 World = mul(InInverseViewProjection, float4(Ndc, NdcDepth, 1));
	return World.xyz / World.w;
}
