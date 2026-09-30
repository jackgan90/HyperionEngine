#pragma once
#include "Hyperion/Math/Math.h"

namespace Hyperion
{
inline constexpr float DegreesPerRadian = 57.2957795f;

inline FVec3 RadiansToDegrees(FVec3 InRadians)
{
	return {InRadians.X * DegreesPerRadian, InRadians.Y * DegreesPerRadian, InRadians.Z * DegreesPerRadian};
}

inline FVec3 DegreesToRadians(FVec3 InDegrees)
{
	return {InDegrees.X / DegreesPerRadian, InDegrees.Y / DegreesPerRadian, InDegrees.Z / DegreesPerRadian};
}
} // namespace Hyperion
