#include "../Material/Parameters.hlsli"

float3 EvaluateDirectLighting(FMaterialParameters InMaterial, float3 InWorld, float3 InCamera, float3 InLight,
                              float3 InLightColor)
{
	float3 N = InMaterial.Normal;
	float3 V = normalize(InCamera - InWorld);
	float3 L = InLight;
	float3 H = (V + L) * rsqrt(max(dot(V + L, V + L), 1e-12));
	float Nl = saturate(dot(N, L));
	float Nv = max(saturate(dot(N, V)), .0001);
	float Nh = saturate(dot(N, H));
	float Vh = saturate(dot(V, H));
	float A = InMaterial.Roughness * InMaterial.Roughness;
	float A2 = A * A;
	float Denominator = Nh * Nh * (A2 - 1) + 1;
	float Distribution = A2 / max(3.14159265 * Denominator * Denominator, .000001);
	float VisibilityV = 2 * Nv / max(Nv + sqrt(A2 + (1 - A2) * Nv * Nv), .0001);
	float VisibilityL = 2 * Nl / max(Nl + sqrt(A2 + (1 - A2) * Nl * Nl), .0001);
	float3 F0 = lerp(float3(.04, .04, .04), InMaterial.BaseColor, InMaterial.Metallic);
	float3 Fresnel = F0 + (1 - F0) * pow(1 - Vh, 5);
	float3 Specular = Distribution * VisibilityV * VisibilityL * Fresnel / max(4 * Nv * Nl, .0001);
	float3 Diffuse = (1 - Fresnel) * (1 - InMaterial.Metallic) * InMaterial.BaseColor / 3.14159265;
	return (Diffuse + Specular) * InLightColor * Nl;
}
