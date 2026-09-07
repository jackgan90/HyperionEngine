#include "Hyperion/Math/Bounds.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
bool IsFinite(FVec3 InValue)
{
	return std::isfinite(InValue.X) && std::isfinite(InValue.Y) && std::isfinite(InValue.Z);
}

bool IsAffine(const FMat4& InMatrix)
{
	const auto& M = InMatrix.Values;
	return std::all_of(M.begin(), M.end(),
	                   [](float InValue)
	                   {
		                   return std::isfinite(InValue);
	                   }) &&
	       M[3] == 0 && M[7] == 0 && M[11] == 0 && M[15] == 1;
}

bool IsUsable(const FBounds& InBounds)
{
	return InBounds.bValid && IsFinite(InBounds.Minimum) && IsFinite(InBounds.Maximum) &&
	       InBounds.Minimum.X <= InBounds.Maximum.X && InBounds.Minimum.Y <= InBounds.Maximum.Y &&
	       InBounds.Minimum.Z <= InBounds.Maximum.Z;
}

FBounds UnionBounds(const FBounds& InA, const FBounds& InB)
{
	if (!IsUsable(InA) || !IsUsable(InB))
	{
		return {};
	}
	return {{std::min(InA.Minimum.X, InB.Minimum.X), std::min(InA.Minimum.Y, InB.Minimum.Y),
	         std::min(InA.Minimum.Z, InB.Minimum.Z)},
	        {std::max(InA.Maximum.X, InB.Maximum.X), std::max(InA.Maximum.Y, InB.Maximum.Y),
	         std::max(InA.Maximum.Z, InB.Maximum.Z)},
	        true};
}

FVec3 BoundsCorner(const FBounds& InBounds, unsigned InCorner)
{
	return {InCorner & 1 ? InBounds.Maximum.X : InBounds.Minimum.X,
	        InCorner & 2 ? InBounds.Maximum.Y : InBounds.Minimum.Y,
	        InCorner & 4 ? InBounds.Maximum.Z : InBounds.Minimum.Z};
}

FBounds TransformBounds(const FBounds& InBounds, const FMat4& InWorld)
{
	if (!IsUsable(InBounds) || !IsAffine(InWorld))
	{
		return {};
	}
	FBounds Result;
	for (unsigned Corner = 0; Corner < 8; ++Corner)
	{
		const auto Point = BoundsCorner(InBounds, Corner);
		const auto P = Transform(InWorld, {Point.X, Point.Y, Point.Z, 1});
		const FBounds Single{{P.X, P.Y, P.Z}, {P.X, P.Y, P.Z}, true};
		if (!IsUsable(Single))
		{
			return {};
		}
		Result = Corner ? UnionBounds(Result, Single) : Single;
	}
	return Result;
}

float BoundsArea(const FBounds& InBounds)
{
	const auto Extent = Subtract(InBounds.Maximum, InBounds.Minimum);
	return 2 * (Extent.X * Extent.Y + Extent.X * Extent.Z + Extent.Y * Extent.Z);
}

FFrustum::FFrustum(const FMat4& InViewProjection)
{
	const auto& M = InViewProjection.Values;
	bValid = std::all_of(M.begin(), M.end(),
	                     [](float InValue)
	                     {
		                     return std::isfinite(InValue);
	                     });
	for (unsigned Index = 0; Index < 6; ++Index)
	{
		const unsigned Row = Index / 2;
		const float Sign = Index % 2 ? -1.f : 1.f;
		Planes[Index] = {M[3] + Sign * M[Row], M[7] + Sign * M[Row + 4], M[11] + Sign * M[Row + 8],
		                 M[15] + Sign * M[Row + 12]};
	}
	Planes[4] = {M[2], M[6], M[10], M[14]}; // Zero-to-one near plane.
	for (const auto& Plane : Planes)
	{
		bValid = bValid && IsFinite({Plane.X, Plane.Y, Plane.Z}) && std::isfinite(Plane.W);
	}
}

bool FFrustum::Intersects(const FBounds& InBounds) const
{
	if (!bValid || !IsUsable(InBounds))
	{
		return true;
	}
	for (const auto& Plane : Planes)
	{
		const auto P =
		    BoundsCorner(InBounds, (Plane.X >= 0 ? 1u : 0u) | (Plane.Y >= 0 ? 2u : 0u) | (Plane.Z >= 0 ? 4u : 0u));
		const double X = double(Plane.X) * P.X;
		const double Y = double(Plane.Y) * P.Y;
		const double Z = double(Plane.Z) * P.Z;
		const double Distance = X + Y + Z + Plane.W;
		const double Epsilon = 1e-5 * (1 + std::abs(X) + std::abs(Y) + std::abs(Z) + std::abs(double(Plane.W)));
		if (std::isfinite(Distance) && Distance < -Epsilon)
		{
			return false;
		}
	}
	return true;
}
} // namespace Hyperion
