#include "Parameters.hlsli"

FMaterialParameters InitMaterialParameters(FVertexOutput InInput, bool bInFront)
{
	FMaterialParameters Material = (FMaterialParameters)0;
	float4 Base = ModelBaseColor(InInput);
	ClipModelAlpha(InInput, Base.a);
	float3 N = normalize(InInput.Normal);
	if (HYP_SURFACE(bDoubleSided) && !bInFront)
	{
		N = -N;
	}
	float3 GeometricNormal = N;
	if (HYP_SURFACE(bHasNormal))
	{
		float3 T = normalize(InInput.Tangent.xyz - N * dot(N, InInput.Tangent.xyz));
		float3 B = cross(N, T) * InInput.Tangent.w;
		float3 Mapped = NormalTexture.Sample(NormalSampler, SelectUv(InInput, HYP_SURFACE(NormalUv))).xyz * 2 - 1;
		Mapped.xy *= HYP_SURFACE(NormalScale);
		N = normalize(T * Mapped.x + B * Mapped.y + N * Mapped.z);
	}
	float4 Mr =
	    MetallicRoughnessTexture.Sample(MetallicRoughnessSampler, SelectUv(InInput, HYP_SURFACE(MetallicRoughnessUv)));
	Material.Metallic = saturate(HYP_SURFACE(Metallic) * Mr.b);
	Material.Roughness = clamp(HYP_SURFACE(Roughness) * Mr.g, .045, 1);
	Material.Normal = N;
	Material.GeometricNormal = GeometricNormal;
	Material.BaseColor = Base.rgb;
	Material.Alpha = HYP_SURFACE(AlphaMode) == 2 ? Base.a : 1;
	Material.bUnlit = HYP_SURFACE(bUnlit);
	Material.Occlusion =
	    lerp(1, OcclusionTexture.Sample(OcclusionSampler, SelectUv(InInput, HYP_SURFACE(OcclusionUv))).r,
	         HYP_SURFACE(OcclusionStrength));
	Material.Emissive =
	    HYP_SURFACE(Emissive) * EmissiveTexture.Sample(EmissiveSampler, SelectUv(InInput, HYP_SURFACE(EmissiveUv))).rgb;
	return Material;
}
