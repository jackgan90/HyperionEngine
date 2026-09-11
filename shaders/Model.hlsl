#define HYP_MATERIAL_VIEW_V1
#define HYP_MATERIAL_OBJECT_V1
#define HYP_MATERIAL_SURFACE_V1
#define HYP_MATERIAL_SCENE_V1
#include "MaterialBlocks.hlsli"
#ifndef HYP_SHADOW_CASTER
#define HYP_SHADOW_CASTER 0
#endif

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
#if HYP_ENABLE_INSTANCE
	nointerpolation uint InstanceId : TEXCOORD5;
#endif
	float4 Position : SV_Position;
#if !HYP_SHADOW_CASTER
	float3 WorldPosition : TEXCOORD2;
	float3 Normal : TEXCOORD3;
	float4 Tangent : TEXCOORD4;
#endif
	float4 Color : COLOR0;
	float2 Uv0 : TEXCOORD0;
	float2 Uv1 : TEXCOORD1;
};

FVertexOutput VSMain(FVertexInput InInput
#if HYP_ENABLE_INSTANCE
                     ,
                     uint InInstanceId : SV_InstanceID
#endif
)
{
	FVertexOutput Output;
#if HYP_ENABLE_INSTANCE
	Output.InstanceId = InInstanceId;
#define HYP_OBJECT(Field) ObjectInstances[InInstanceId].Field
#else
#define HYP_OBJECT(Field) Field
#endif
	float4 Position = mul(HYP_OBJECT(World), float4(InInput.Position, 1));
	Output.Position = mul(ViewProjection, Position);
#if !HYP_SHADOW_CASTER
	Output.WorldPosition = Position.xyz;
	Output.Normal = mul((float3x3)HYP_OBJECT(Normal), InInput.Normal);
	Output.Tangent =
	    float4(mul((float3x3)HYP_OBJECT(World), InInput.Tangent.xyz), InInput.Tangent.w * HYP_OBJECT(OrientationSign));
#endif
	Output.Color = InInput.Color;
	Output.Uv0 = InInput.Uv0;
	Output.Uv1 = InInput.Uv1;
	return Output;
}

float2 SelectUv(FVertexOutput InInput, float InSet)
{
	return InSet > .5 ? InInput.Uv1 : InInput.Uv0;
}

#if HYP_ENABLE_INSTANCE
#define HYP_SURFACE(Field) SurfaceInstances[InInput.InstanceId].Field
#else
#define HYP_SURFACE(Field) Field
#endif

float4 ModelBaseColor(FVertexOutput InInput)
{
	return HYP_SURFACE(BaseColor) * InInput.Color *
	       BaseColorTexture.Sample(BaseColorSampler, SelectUv(InInput, HYP_SURFACE(BaseColorUv)));
}

void ClipModelAlpha(FVertexOutput InInput, float InAlpha)
{
	if (HYP_SURFACE(AlphaMode) == 1)
	{
		clip(InAlpha - HYP_SURFACE(AlphaCutoff));
	}
}

#if HYP_SHADOW_CASTER
void PSMain(FVertexOutput InInput)
{
	ClipModelAlpha(InInput, ModelBaseColor(InInput).a);
}
#else
#include "Material/ModelMaterial.hlsli"
#include "Material/Parameters.hlsli"
#if HYP_DEFERRED_BASE
#include "Deferred/GBuffer.hlsli"

FGBufferOutput PSMain(FVertexOutput InInput, bool bInFront : SV_IsFrontFace)
{
	FMaterialParameters Material = InitMaterialParameters(InInput, bInFront);
	clip(Material.bUnlit ? -1 : 1);
	return EncodeGBuffer(Material);
}
#else
#include "Lighting/SurfaceLighting.hlsli"

float4 PSMain(FVertexOutput InInput, bool bInFront : SV_IsFrontFace) : SV_Target0
{
	FMaterialParameters Material = InitMaterialParameters(InInput, bInFront);
#if HYP_UNLIT_COMPATIBILITY
	clip(Material.bUnlit ? 1 : -1);
#endif
	float3 Color =
	    EvaluateLighting(Material, InInput.WorldPosition, CameraPosition.xyz, MainLightDirection, MainLightColor,
	                     AmbientColor, ddx(InInput.WorldPosition), ddy(InInput.WorldPosition));
#if !HYP_FORWARD_HDR
	if (!Material.bUnlit)
	{
		Color = Color / (1 + Color);
	}
#endif
	return float4(Color, Material.Alpha);
}
#endif
#endif
