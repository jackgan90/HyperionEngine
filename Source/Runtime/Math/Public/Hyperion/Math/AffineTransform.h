#pragma once
#include "Hyperion/Math/Math.h"

namespace Hyperion
{
// A = Rz * Ry * Rx * U, where U has Scale on its diagonal and Shear = (Uxy, Uxz, Uyz).
// Angles are right-handed radians about fixed X, then Y, then Z axes.
// Full-rank reflections use signed X scale. Singular matrices have a deterministic completed basis.
struct FAffineTransform
{
	FVec3 Position;
	FVec3 Rotation;
	FVec3 Scale{1, 1, 1};
	FVec3 Shear;
};

FAffineTransform DecomposeAffine(const FMat4& InMatrix);
FMat4 ComposeAffine(const FAffineTransform& InTransform);
} // namespace Hyperion
