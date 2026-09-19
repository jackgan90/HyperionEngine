#include "Hyperion/Math/Ray.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
bool IsUsable(const FRay& InRay)
{
	return IsFinite(InRay.Origin) && IsFinite(InRay.Direction) &&
	       (InRay.Direction.X != 0 || InRay.Direction.Y != 0 || InRay.Direction.Z != 0) &&
	       std::isfinite(InRay.Minimum) && std::isfinite(InRay.Maximum) && InRay.Minimum >= 0 &&
	       InRay.Maximum >= InRay.Minimum;
}

std::optional<float> IntersectRayBounds(const FRay& InRay, const FBounds& InBounds)
{
	if (!IsUsable(InRay))
	{
		return {};
	}
	if (!IsUsable(InBounds))
	{
		return InRay.Minimum;
	}
	const std::array<double, 3> Origin{InRay.Origin.X, InRay.Origin.Y, InRay.Origin.Z};
	const std::array<double, 3> Direction{InRay.Direction.X, InRay.Direction.Y, InRay.Direction.Z};
	const std::array<double, 3> Minimum{InBounds.Minimum.X, InBounds.Minimum.Y, InBounds.Minimum.Z};
	const std::array<double, 3> Maximum{InBounds.Maximum.X, InBounds.Maximum.Y, InBounds.Maximum.Z};
	double Near = InRay.Minimum;
	double Far = InRay.Maximum;
	for (std::size_t Axis = 0; Axis < 3; ++Axis)
	{
		if (Direction[Axis] == 0)
		{
			if (Origin[Axis] < Minimum[Axis] || Origin[Axis] > Maximum[Axis])
			{
				return {};
			}
			continue;
		}
		const double A = (Minimum[Axis] - Origin[Axis]) / Direction[Axis];
		const double B = (Maximum[Axis] - Origin[Axis]) / Direction[Axis];
		Near = std::max(Near, std::min(A, B));
		Far = std::min(Far, std::max(A, B));
		if (Near > Far)
		{
			return {};
		}
	}
	return static_cast<float>(Near);
}

std::optional<FRayTriangleHit> IntersectRayTriangle(const FRay& InRay, FVec3 InA, FVec3 InB, FVec3 InC)
{
	// Double intermediates avoid scale-dependent epsilon rejection of small triangles.
	const std::array<double, 3> Edge1{double(InB.X) - InA.X, double(InB.Y) - InA.Y, double(InB.Z) - InA.Z};
	const std::array<double, 3> Edge2{double(InC.X) - InA.X, double(InC.Y) - InA.Y, double(InC.Z) - InA.Z};
	const std::array<double, 3> Offset{double(InRay.Origin.X) - InA.X, double(InRay.Origin.Y) - InA.Y,
	                                   double(InRay.Origin.Z) - InA.Z};
	const std::array<double, 3> P{InRay.Direction.Y * Edge2[2] - InRay.Direction.Z * Edge2[1],
	                              InRay.Direction.Z * Edge2[0] - InRay.Direction.X * Edge2[2],
	                              InRay.Direction.X * Edge2[1] - InRay.Direction.Y * Edge2[0]};
	const double Det = Edge1[0] * P[0] + Edge1[1] * P[1] + Edge1[2] * P[2];
	if (Det == 0 || !std::isfinite(Det))
	{
		return {};
	}
	const double U = (Offset[0] * P[0] + Offset[1] * P[1] + Offset[2] * P[2]) / Det;
	const std::array<double, 3> Q{Offset[1] * Edge1[2] - Offset[2] * Edge1[1],
	                              Offset[2] * Edge1[0] - Offset[0] * Edge1[2],
	                              Offset[0] * Edge1[1] - Offset[1] * Edge1[0]};
	const double V = (InRay.Direction.X * Q[0] + InRay.Direction.Y * Q[1] + InRay.Direction.Z * Q[2]) / Det;
	const double Distance = (Edge2[0] * Q[0] + Edge2[1] * Q[1] + Edge2[2] * Q[2]) / Det;
	if (U < 0 || V < 0 || U + V > 1 || Distance < InRay.Minimum || Distance > InRay.Maximum || !std::isfinite(Distance))
	{
		return {};
	}
	return FRayTriangleHit{static_cast<float>(Distance),
	                       {static_cast<float>(1 - U - V), static_cast<float>(U), static_cast<float>(V)},
	                       Det > 0};
}

FRay TransformRay(const FRay& InRay, const FMat4& InTransform)
{
	const auto Origin = Transform(InTransform, {InRay.Origin.X, InRay.Origin.Y, InRay.Origin.Z, 1});
	const auto Direction = Transform(InTransform, {InRay.Direction.X, InRay.Direction.Y, InRay.Direction.Z, 0});
	return {{Origin.X, Origin.Y, Origin.Z}, {Direction.X, Direction.Y, Direction.Z}, InRay.Minimum, InRay.Maximum};
}
} // namespace Hyperion
