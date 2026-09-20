#pragma once
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
enum class EPrimitiveShape
{
	Cube,
	Sphere,
	Cylinder,
	Cone,
	Plane
};

// Original unit-sized CPU geometry, centered at the origin; vertical axis is +Y.
FModelPrimitive MakePrimitiveShape(EPrimitiveShape InShape);
} // namespace Hyperion
