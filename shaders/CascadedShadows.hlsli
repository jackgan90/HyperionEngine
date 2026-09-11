#ifndef HYP_CASCADED_SHADOWS_V1
#define HYP_CASCADED_SHADOWS_V1

cbuffer ShadowViewV1 : register(b4)
{
	column_major float4x4 ShadowMatrix0;
	column_major float4x4 ShadowMatrix1;
	column_major float4x4 ShadowMatrix2;
	column_major float4x4 ShadowMatrix3;
	float4 ShadowSplits;
	float4 ShadowTexels;
	float4 ShadowRanges;
	float4 ShadowCamera;
	float4 ShadowFilter;
	float4 ShadowControl;
};

Texture2D<float> ShadowDepth0 : register(t5);
Texture2D<float> ShadowDepth1 : register(t6);
Texture2D<float> ShadowDepth2 : register(t7);
Texture2D<float> ShadowDepth3 : register(t8);
SamplerComparisonState ShadowSampler : register(s5);

float4x4 ShadowMatrix(uint InCascade)
{
	if (InCascade == 0)
	{
		return ShadowMatrix0;
	}
	if (InCascade == 1)
	{
		return ShadowMatrix1;
	}
	if (InCascade == 2)
	{
		return ShadowMatrix2;
	}
	return ShadowMatrix3;
}

float CompareShadow(uint InCascade, float2 InUv, float InDepth)
{
	if (InCascade == 0)
	{
		return ShadowDepth0.SampleCmpLevelZero(ShadowSampler, InUv, InDepth);
	}
	if (InCascade == 1)
	{
		return ShadowDepth1.SampleCmpLevelZero(ShadowSampler, InUv, InDepth);
	}
	if (InCascade == 2)
	{
		return ShadowDepth2.SampleCmpLevelZero(ShadowSampler, InUv, InDepth);
	}
	return ShadowDepth3.SampleCmpLevelZero(ShadowSampler, InUv, InDepth);
}

float CascadeVisibility(uint InCascade, float3 InWorld, float3 InNormal, float3 InLight, float3 InDx, float3 InDy)
{
	float WorldTexel = ShadowTexels[InCascade];
	float DepthTexel = WorldTexel / max(ShadowRanges[InCascade], .0001);
	float Sine = sqrt(saturate(1 - pow(saturate(dot(InNormal, InLight)), 2)));
	float3 World = InWorld + InNormal * (WorldTexel * ShadowFilter.y * Sine);
	float4x4 Matrix = ShadowMatrix(InCascade);
	float3 Projected = mul(Matrix, float4(World, 1)).xyz;
	float2 Uv = Projected.xy * float2(.5, -.5) + .5;
	float Guard = ShadowControl.z * 2;
	if (any(Uv < Guard) || any(Uv > 1 - Guard) || Projected.z <= 0 || Projected.z >= 1)
	{
		return 1;
	}
	float3 Dx = mul(Matrix, float4(InDx, 0)).xyz * float3(.5, -.5, 1);
	float3 Dy = mul(Matrix, float4(InDy, 0)).xyz * float3(.5, -.5, 1);
	float Det = Dx.x * Dy.y - Dx.y * Dy.x;
	float2 Gradient = 0;
	if (abs(Det) > 1e-10)
	{
		Gradient = float2(Dx.z * Dy.y - Dy.z * Dx.y, Dy.z * Dx.x - Dx.z * Dy.x) / Det;
	}
	// Grazing receivers must never create arbitrarily large plane corrections.
	float Limit = 2 * DepthTexel / max(ShadowControl.z, .000001);
	Gradient = clamp(Gradient, -Limit, Limit);
	float Visibility = 0;
	[unroll] for (int Y = -1; Y <= 1; ++Y)
	{
		[unroll] for (int X = -1; X <= 1; ++X)
		{
			float2 Offset = float2(X, Y) * ShadowControl.z;
			// ShadowFilter.x is signed for the shadow depth convention; DepthTexel is a positive distance.
			float Depth = Projected.z + dot(Gradient, Offset) - DepthTexel * ShadowFilter.x;
			Visibility += CompareShadow(InCascade, Uv + Offset, saturate(Depth));
		}
	}
	return Visibility / 9;
}

uint SelectCascade(float InDepth)
{
	return (InDepth > ShadowSplits.x) + (InDepth > ShadowSplits.y) + (InDepth > ShadowSplits.z);
}

float DirectionalShadow(float3 InWorld, float3 InNormal, float3 InLight, float3 Dx, float3 Dy)
{
	float Depth = dot(ShadowCamera, float4(InWorld, 1));
	if (ShadowControl.x < .5 || Depth <= 0 || Depth >= ShadowControl.w)
	{
		return 1;
	}
	uint Cascade = SelectCascade(Depth);
	float Visibility = CascadeVisibility(Cascade, InWorld, InNormal, InLight, Dx, Dy);
	float Previous = Cascade == 0 ? 0 : ShadowSplits[Cascade - 1];
	float Width = max((ShadowSplits[Cascade] - Previous) * ShadowFilter.z, .001);
	float Blend = saturate((Depth - ShadowSplits[Cascade] + Width) / Width);
	if (Cascade < 3 && Blend > 0)
	{
		Visibility = lerp(Visibility, CascadeVisibility(Cascade + 1, InWorld, InNormal, InLight, Dx, Dy), Blend);
	}
	float Fade = saturate((ShadowControl.w - Depth) / max(ShadowControl.w * ShadowFilter.w, .001));
	return lerp(1, Visibility, Fade);
}

float DirectionalShadow(float3 InWorld, float3 InNormal, float3 InLight)
{
	return DirectionalShadow(InWorld, InNormal, InLight, ddx(InWorld), ddy(InWorld));
}

float3 ShadowDebugColor(float3 InColor, float3 InWorld)
{
	if (ShadowControl.x < .5 || ShadowControl.y < .5)
	{
		return InColor;
	}
	uint Cascade = SelectCascade(dot(ShadowCamera, float4(InWorld, 1)));
	float3 Colors[4] = {float3(1, .2, .2), float3(.2, 1, .2), float3(.2, .4, 1), float3(1, .8, .2)};
	return lerp(InColor, Colors[Cascade], .4);
}
#endif
