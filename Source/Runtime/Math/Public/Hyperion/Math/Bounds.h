#pragma once
#include "Hyperion/Math/Math.h"

namespace Hyperion
{
struct FBounds
{
	FVec3 Minimum;
	FVec3 Maximum;
	bool bValid{};
};

bool IsFinite(FVec3 InValue);
bool IsAffine(const FMat4& InMatrix);
bool IsUsable(const FBounds& InBounds);
FBounds UnionBounds(const FBounds& InA, const FBounds& InB);
FBounds TransformBounds(const FBounds& InBounds, const FMat4& InWorld);
FVec3 BoundsCorner(const FBounds& InBounds, unsigned InCorner);
float BoundsArea(const FBounds& InBounds);

class FFrustum
{
public:
	explicit FFrustum(const FMat4& InViewProjection);
	bool Intersects(const FBounds& InBounds) const;

private:
	std::array<FVec4, 6> Planes;
	bool bValid = true;
};
} // namespace Hyperion
