cbuffer ModelConstants : register(b0)
{
	column_major float4x4 World;
	column_major float4x4 ViewProjection;
	column_major float4x4 NormalTransform;
	float4 CameraPosition;
	float4 BaseColorFactor;
	float4 EmissiveAndNormal;
	float4 Pbr;
	float4 Modes;
	float4 UvSets;
	float4 Extra;
};

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
	Output.Normal = mul((float3x3)NormalTransform, InInput.Normal);
	Output.Tangent = float4(mul((float3x3)World, InInput.Tangent.xyz), InInput.Tangent.w * Modes.w);
	Output.Color = InInput.Color;
	Output.Uv0 = InInput.Uv0;
	Output.Uv1 = InInput.Uv1;
	return Output;
}

float2 SelectUv(FVertexOutput InInput, float InSet)
{
	return InSet > .5 ? InInput.Uv1 : InInput.Uv0;
}

float4 PSMain(FVertexOutput InInput, bool InFront : SV_IsFrontFace) : SV_Target0
{
	float4 Base =
	    BaseColorFactor * InInput.Color * BaseColorTexture.Sample(BaseColorSampler, SelectUv(InInput, UvSets.x));
	if (Modes.x > .5 && Modes.x < 1.5)
	{
		clip(Base.a - Pbr.w);
	}
	float Alpha = Modes.x > 1.5 ? Base.a : 1;
	if (Modes.z > .5)
	{
		return float4(Base.rgb, Alpha);
	}
	float3 N = normalize(InInput.Normal);
	if (Modes.y > .5 && !InFront)
	{
		N = -N;
	}
	if (Extra.y > .5)
	{
		float3 T = normalize(InInput.Tangent.xyz - N * dot(N, InInput.Tangent.xyz));
		float3 B = cross(N, T) * InInput.Tangent.w;
		float3 Mapped = NormalTexture.Sample(NormalSampler, SelectUv(InInput, UvSets.z)).xyz * 2 - 1;
		Mapped.xy *= EmissiveAndNormal.w;
		N = normalize(T * Mapped.x + B * Mapped.y + N * Mapped.z);
	}
	float4 Mr = MetallicRoughnessTexture.Sample(MetallicRoughnessSampler, SelectUv(InInput, UvSets.y));
	float Metallic = saturate(Pbr.x * Mr.b);
	float Roughness = clamp(Pbr.y * Mr.g, .045, 1);
	float3 V = normalize(CameraPosition.xyz - InInput.WorldPosition);
	float3 L = normalize(float3(-.45, .8, .65));
	float3 H = normalize(V + L);
	float Nl = saturate(dot(N, L));
	float Nv = max(saturate(dot(N, V)), .0001);
	float Nh = saturate(dot(N, H));
	float Vh = saturate(dot(V, H));
	float A = Roughness * Roughness;
	float A2 = A * A;
	float Denominator = Nh * Nh * (A2 - 1) + 1;
	float Distribution = A2 / max(3.14159265 * Denominator * Denominator, .000001);
	float VisibilityV = 2 * Nv / max(Nv + sqrt(A2 + (1 - A2) * Nv * Nv), .0001);
	float VisibilityL = 2 * Nl / max(Nl + sqrt(A2 + (1 - A2) * Nl * Nl), .0001);
	float3 F0 = lerp(float3(.04, .04, .04), Base.rgb, Metallic);
	float3 Fresnel = F0 + (1 - F0) * pow(1 - Vh, 5);
	float3 Specular = Distribution * VisibilityV * VisibilityL * Fresnel / max(4 * Nv * Nl, .0001);
	float3 Diffuse = (1 - Fresnel) * (1 - Metallic) * Base.rgb / 3.14159265;
	float Occlusion = lerp(1, OcclusionTexture.Sample(OcclusionSampler, SelectUv(InInput, UvSets.w)).r, Pbr.z);
	float3 Ambient = (Base.rgb * (1 - Metallic) + F0 * (.7 - .4 * Roughness)) * float3(.22, .25, .3) * Occlusion;
	float3 Emissive = EmissiveAndNormal.xyz * EmissiveTexture.Sample(EmissiveSampler, SelectUv(InInput, Extra.x)).rgb;
	float3 Color = (Diffuse + Specular) * float3(3, 2.85, 2.7) * Nl + Ambient + Emissive;
	Color = Color / (1 + Color);
	return float4(Color, Alpha);
}
