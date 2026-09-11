#include "../Material/Parameters.hlsli"

// Layout version 1: reserve Surface.w; shading model dispatch can later use stencil.
float2 EncodeNormal(float3 InNormal)
{
	float3 N = InNormal / (abs(InNormal.x) + abs(InNormal.y) + abs(InNormal.z));
	float2 Sign = float2(N.x >= 0 ? 1 : -1, N.y >= 0 ? 1 : -1);
	return (N.z >= 0 ? N.xy : (1 - abs(N.yx)) * Sign) * .5 + .5;
}

float3 DecodeNormal(float2 InEncoded)
{
	float2 P = InEncoded * 2 - 1;
	float3 N = float3(P, 1 - abs(P.x) - abs(P.y));
	float T = saturate(-N.z);
	N.xy += float2(N.x >= 0 ? -T : T, N.y >= 0 ? -T : T);
	return normalize(N);
}

struct FGBufferOutput
{
	float4 BaseMetallic : SV_Target0;
	float4 Normals : SV_Target1;
	float4 Surface : SV_Target2;
	float4 Emissive : SV_Target3;
};

FGBufferOutput EncodeGBuffer(FMaterialParameters InMaterial)
{
	FGBufferOutput Result;
	Result.BaseMetallic = float4(InMaterial.BaseColor, InMaterial.Metallic);
	Result.Normals = float4(EncodeNormal(InMaterial.Normal), EncodeNormal(InMaterial.GeometricNormal));
	Result.Surface = float4(InMaterial.Roughness, InMaterial.Occlusion, 1, 0);
	Result.Emissive = float4(InMaterial.Emissive, 0);
	return Result;
}

FMaterialParameters DecodeGBuffer(float4 InBase, float4 InNormals, float4 InSurface, float4 InEmissive)
{
	FMaterialParameters Result = (FMaterialParameters)0;
	Result.BaseColor = InBase.rgb;
	Result.Metallic = InBase.a;
	Result.Normal = DecodeNormal(InNormals.xy);
	Result.GeometricNormal = DecodeNormal(InNormals.zw);
	Result.Roughness = InSurface.x;
	Result.Occlusion = InSurface.y;
	Result.Emissive = InEmissive.rgb;
	Result.Alpha = 1;
	return Result;
}
