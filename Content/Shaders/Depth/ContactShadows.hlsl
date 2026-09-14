#include "../Deferred/GBuffer.hlsli"

Texture2D<float> HierarchicalDepth : register(t0);
Texture2D<float4> SurfaceNormals : register(t1);
Texture2D<float4> SurfaceCoverage : register(t2);

cbuffer ContactV1 : register(b0)
{
	column_major float4x4 ViewProjection;
	column_major float4x4 InverseViewProjection;
	float4 Viewport;
	float3 Eye;
	float3 LightDirection;
	float RayLength;
	float Thickness;
	float Bias;
	uint MaxSteps;
	uint bReversed;
};

float3 ContactWorld(float2 InPixel, float InDepth)
{
	float2 Ndc = ((InPixel - Viewport.xy) / Viewport.zw) * float2(2, -2) + float2(-1, 1);
	float4 World = mul(InverseViewProjection, float4(Ndc, InDepth, 1));
	return World.xyz / World.w;
}

float ClipRayEnd(float4 InStart, float4 InEnd)
{
	float First[6] = {
	    InStart.w + InStart.x, InStart.w - InStart.x, InStart.w + InStart.y, InStart.w - InStart.y, InStart.z,
	    InStart.w - InStart.z};
	float Last[6] = {InEnd.w + InEnd.x, InEnd.w - InEnd.x, InEnd.w + InEnd.y,
	                 InEnd.w - InEnd.y, InEnd.z,           InEnd.w - InEnd.z};
	float Limit = 1;
	[unroll] for (uint Plane = 0; Plane < 6; ++Plane)
	{
		if (First[Plane] < 0)
		{
			return 0;
		}
		if (Last[Plane] < 0)
		{
			Limit = min(Limit, First[Plane] / max(First[Plane] - Last[Plane], 1e-8));
		}
	}
	return Limit;
}

struct FContactRay
{
	float3 Start;
	float3 Delta;
	float3 WorldStart;
	float3 WorldDelta;
	float2 ClipW;
	float2 Size;
	float Epsilon;
};

float WorldFraction(FContactRay InRay, float InScreenFraction)
{
	return InScreenFraction * InRay.ClipW.x / max(lerp(InRay.ClipW.y, InRay.ClipW.x, InScreenFraction), 1e-8);
}

float CellEnd(FContactRay InRay, uint2 InCell, uint2 InSize, float InStart)
{
	float2 Boundary = (float2(InCell) + float2(InRay.Delta.x >= 0 ? 1 : 0, InRay.Delta.y >= 0 ? 1 : 0)) / InSize;
	float2 Exit;
	Exit.x = abs(InRay.Delta.x) > 1e-10 ? (Boundary.x - InRay.Start.x) / InRay.Delta.x : 2;
	Exit.y = abs(InRay.Delta.y) > 1e-10 ? (Boundary.y - InRay.Start.y) / InRay.Delta.y : 2;
	return min(1, max(InStart + InRay.Epsilon, min(Exit.x, Exit.y)));
}

float ConfirmContact(FContactRay InRay, float InStart, float InEnd, float InDepth, int2 InCell, int2 InReceiver,
                     float3 InWorld, float3 InNormal)
{
	if (all(InCell == InReceiver) || SurfaceCoverage.Load(int3(InCell, 0)).z < .5 ||
	    (bReversed ? InDepth <= 0 : InDepth >= 1))
	{
		return 1;
	}
	float Hit = abs(InRay.Delta.z) > 1e-9 ? clamp((InDepth - InRay.Start.z) / InRay.Delta.z, InStart, InEnd) : InEnd;
	float2 Pixel = (InRay.Start.xy + InRay.Delta.xy * Hit) * InRay.Size;
	float3 SceneWorld = ContactWorld(Pixel, InDepth);
	float Fraction = WorldFraction(InRay, Hit);
	float3 RayWorld = InRay.WorldStart + InRay.WorldDelta * Fraction;
	float Gap = dot(RayWorld - SceneWorld, normalize(RayWorld - Eye));
	// Reject the receiver's own plane while preserving nearby distinct surfaces.
	float3 HitNormal = DecodeNormal(SurfaceNormals.Load(int3(InCell, 0)).zw);
	if (dot(HitNormal, InNormal) > .995 && abs(dot(SceneWorld - InWorld, InNormal)) < max(Bias * 2, 1e-4))
	{
		return 1;
	}
	float Distance = length(RayWorld - InRay.WorldStart);
	if (Gap < -1e-5 || Gap > Thickness || Distance < Bias)
	{
		return 1;
	}
	float2 Edge = min(Pixel - Viewport.xy, Viewport.xy + Viewport.zw - Pixel);
	float Strength = saturate(min(Edge.x, Edge.y) / 8) * (1 - smoothstep(RayLength * .8, RayLength, Distance));
	return 1 - Strength;
}

float TraceContact(FContactRay InRay, uint InLevels, int2 InReceiver, float3 InWorld, float3 InNormal)
{
	float Span = max(abs(InRay.Delta.x) * InRay.Size.x, abs(InRay.Delta.y) * InRay.Size.y);
	if (Span < .5)
	{
		return 1;
	}
	uint TopMip = min(InLevels - 1, uint(max(0, floor(log2(Span)))));
	uint Mip = TopMip;
	float Position = 0;
	[loop] for (uint Step = 0; Step < MaxSteps && Position < 1; ++Step)
	{
		uint Width;
		uint Height;
		uint Levels;
		HierarchicalDepth.GetDimensions(Mip, Width, Height, Levels);
		float2 Uv = InRay.Start.xy + InRay.Delta.xy * Position;
		if (any(Uv < 0) || any(Uv >= 1))
		{
			break;
		}
		uint2 Cell = min(uint2(Uv * uint2(Width, Height)), uint2(Width, Height) - 1);
		float End = CellEnd(InRay, Cell, uint2(Width, Height), Position);
		float Depth = HierarchicalDepth.Load(int3(Cell, Mip));
		float2 RayDepth = InRay.Start.z + InRay.Delta.z * float2(Position, End);
		bool bPossible = bReversed ? min(RayDepth.x, RayDepth.y) <= Depth : max(RayDepth.x, RayDepth.y) >= Depth;
		if (bPossible && Mip > 0)
		{
			--Mip;
			continue;
		}
		if (bPossible)
		{
			float Visibility = ConfirmContact(InRay, Position, End, Depth, int2(Cell), InReceiver, InWorld, InNormal);
			if (Visibility < 1)
			{
				return Visibility;
			}
		}
		Position = End + InRay.Epsilon;
		Mip = min(Mip + 1, TopMip);
	}
	return 1;
}

float PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	int2 Pixel = int2(InPosition.xy);
	if (SurfaceCoverage.Load(int3(Pixel, 0)).z < .5)
	{
		return 1;
	}
	float3 Normal = DecodeNormal(SurfaceNormals.Load(int3(Pixel, 0)).zw);
	float3 Light = normalize(LightDirection);
	if (dot(Normal, Light) <= 0)
	{
		return 1;
	}
	float3 World = ContactWorld(InPosition.xy, HierarchicalDepth.Load(int3(Pixel, 0)));
	float3 Start = World + Normal * Bias + Light * Bias;
	float3 End = Start + Light * RayLength;
	float4 ClipStart = mul(ViewProjection, float4(Start, 1));
	float4 ClipEnd = mul(ViewProjection, float4(End, 1));
	float Limit = ClipRayEnd(ClipStart, ClipEnd);
	if (ClipStart.w <= 1e-6 || Limit <= 0)
	{
		return 1;
	}
	End = lerp(Start, End, Limit * .99999);
	ClipEnd = mul(ViewProjection, float4(End, 1));
	if (ClipEnd.w <= 1e-6)
	{
		return 1;
	}
	uint Width;
	uint Height;
	uint Levels;
	HierarchicalDepth.GetDimensions(0, Width, Height, Levels);
	float3 A = ClipStart.xyz / ClipStart.w;
	float3 B = ClipEnd.xyz / ClipEnd.w;
	FContactRay Ray;
	Ray.Size = float2(Width, Height);
	Ray.Start = float3(((A.xy * float2(.5, -.5) + .5) * Viewport.zw + Viewport.xy) / Ray.Size, A.z);
	Ray.Delta = float3((B.xy - A.xy) * float2(.5, -.5) * Viewport.zw / Ray.Size, B.z - A.z);
	Ray.WorldStart = Start;
	Ray.WorldDelta = End - Start;
	Ray.ClipW = float2(ClipStart.w, ClipEnd.w);
	Ray.Epsilon = min(1e-4, .01 / max(length(Ray.Delta.xy * Ray.Size), 1));
	return TraceContact(Ray, Levels, Pixel, World, Normal);
}
