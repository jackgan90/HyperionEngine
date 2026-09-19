#pragma once
#include "Hyperion/Math/Bounds.h"
#include <limits>
#include <optional>

namespace Hyperion
{
// Direction need not be unit length. Transforming it without normalization preserves T.
struct FRay
{
	FVec3 Origin;
	FVec3 Direction;
	float Minimum = 0;
	float Maximum = std::numeric_limits<float>::max();
};

struct FRayTriangleHit
{
	float Distance{};
	FVec3 Barycentrics;
	bool bFrontFacing{};
};

bool IsUsable(const FRay& InRay);
std::optional<float> IntersectRayBounds(const FRay& InRay, const FBounds& InBounds);
std::optional<FRayTriangleHit> IntersectRayTriangle(const FRay& InRay, FVec3 InA, FVec3 InB, FVec3 InC);
FRay TransformRay(const FRay& InRay, const FMat4& InTransform);
} // namespace Hyperion
