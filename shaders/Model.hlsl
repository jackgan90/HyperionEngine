#define HYP_MATERIAL_VIEW_V1
#define HYP_MATERIAL_OBJECT_V1
#define HYP_MATERIAL_SURFACE_V1
#define HYP_MATERIAL_SCENE_V1
#include "MaterialBlocks.hlsli"

Texture2D BaseColorTexture : register(t0);
Texture2D MetallicRoughnessTexture : register(t1);
Texture2D NormalTexture : register(t2);
Texture2D OcclusionTexture : register(t3);
Texture2D EmissiveTexture : register(t4);
SamplerState BaseColorSampler : register(s0);
SamplerState MetallicRoughnessSampler : register(s1);
SamplerState NormalSampler : register(s2);
SamplerState OcclusionSampler : register(s3);
SamplerState EmissiveSampler : register(s4);

struct FVertexInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Tangent : TANGENT;
	float4 Color : COLOR0;
	float2 Uv0 : TEXCOORD0;
	float2 Uv1 : TEXCOORD1;
};

struct FVertexOutput
{
	float4 Position : SV_Position;
	float3 WorldPosition : TEXCOORD2;
	float3 Normal : TEXCOORD3;
	float4 Tangent : TEXCOORD4;
	float4 Color : COLOR0;
	float2 Uv0 : TEXCOORD0;
	float2 Uv1 : TEXCOORD1;
};

FVertexOutput VSMain(FVertexInput InInput)
{
	FVertexOutput Output;
	float4 Position = mul(World, float4(InInput.Position, 1));
	Output.Position = mul(ViewProjection, Position);
	Output.WorldPosition = Position.xyz;
	Output.Normal = mul((float3x3)Normal, InInput.Normal);
	Output.Tangent = float4(mul((float3x3)World, InInput.Tangent.xyz), InInput.Tangent.w * OrientationSign);
	Output.Color = InInput.Color;
	Output.Uv0 = InInput.Uv0;
	Output.Uv1 = InInput.Uv1;
	return Output;
}

float2 SelectUv(FVertexOutput InInput, float InSet)
{
	return InSet > .5 ? InInput.Uv1 : InInput.Uv0;
}

float4 PSMain(FVertexOutput InInput, bool bInFront : SV_IsFrontFace) : SV_Target0
{
	float4 Base = BaseColor * InInput.Color * BaseColorTexture.Sample(BaseColorSampler, SelectUv(InInput, BaseColorUv));
	if (AlphaMode == 1)
	{
		clip(Base.a - AlphaCutoff);
	}
	float Alpha = AlphaMode == 2 ? Base.a : 1;
	if (bUnlit)
	{
		return float4(Base.rgb, Alpha);
	}
	float3 N = normalize(InInput.Normal);
	if (bDoubleSided && !bInFront)
	{
		N = -N;
	}
	if (bHasNormal)
	{
		float3 T = normalize(InInput.Tangent.xyz - N * dot(N, InInput.Tangent.xyz));
		float3 B = cross(N, T) * InInput.Tangent.w;
		float3 Mapped = NormalTexture.Sample(NormalSampler, SelectUv(InInput, NormalUv)).xyz * 2 - 1;
		Mapped.xy *= NormalScale;
		N = normalize(T * Mapped.x + B * Mapped.y + N * Mapped.z);
	}
	float4 Mr = MetallicRoughnessTexture.Sample(MetallicRoughnessSampler, SelectUv(InInput, MetallicRoughnessUv));
	float SurfaceMetallic = saturate(Metallic * Mr.b);
	float SurfaceRoughness = clamp(Roughness * Mr.g, .045, 1);
	float3 V = normalize(CameraPosition.xyz - InInput.WorldPosition);
	float3 L = MainLightDirection;
	float3 H = normalize(V + L);
	float Nl = saturate(dot(N, L));
	float Nv = max(saturate(dot(N, V)), .0001);
	float Nh = saturate(dot(N, H));
	float Vh = saturate(dot(V, H));
	float A = SurfaceRoughness * SurfaceRoughness;
	float A2 = A * A;
	float Denominator = Nh * Nh * (A2 - 1) + 1;
	float Distribution = A2 / max(3.14159265 * Denominator * Denominator, .000001);
	float VisibilityV = 2 * Nv / max(Nv + sqrt(A2 + (1 - A2) * Nv * Nv), .0001);
	float VisibilityL = 2 * Nl / max(Nl + sqrt(A2 + (1 - A2) * Nl * Nl), .0001);
	float3 F0 = lerp(float3(.04, .04, .04), Base.rgb, SurfaceMetallic);
	float3 Fresnel = F0 + (1 - F0) * pow(1 - Vh, 5);
	float3 Specular = Distribution * VisibilityV * VisibilityL * Fresnel / max(4 * Nv * Nl, .0001);
	float3 Diffuse = (1 - Fresnel) * (1 - SurfaceMetallic) * Base.rgb / 3.14159265;
	float Occlusion =
	    lerp(1, OcclusionTexture.Sample(OcclusionSampler, SelectUv(InInput, OcclusionUv)).r, OcclusionStrength);
	float3 Ambient = (Base.rgb * (1 - SurfaceMetallic) + F0 * (.7 - .4 * SurfaceRoughness)) * AmbientColor * Occlusion;
	float3 SurfaceEmissive = Emissive * EmissiveTexture.Sample(EmissiveSampler, SelectUv(InInput, EmissiveUv)).rgb;
	float3 Color = (Diffuse + Specular) * MainLightColor * Nl + Ambient + SurfaceEmissive;
	Color = Color / (1 + Color);
	return float4(Color, Alpha);
}
