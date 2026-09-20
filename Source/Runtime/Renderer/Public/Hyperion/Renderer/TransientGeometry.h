#pragma once
#include "Hyperion/Renderer/RenderFeatures.h"

namespace Hyperion
{
// Immutable, viewport-owned geometry. Never enters the scene, visibility index or shadow family.
struct FTransientGeometry
{
	std::vector<FRenderPrimitiveState> Items;
	std::shared_ptr<const void> Lifetime;
};

std::unique_ptr<IRenderFeature> MakeTransientGeometryFeature();
std::shared_ptr<const FMaterialSnapshot> MakeGeometryPreviewMaterial();
} // namespace Hyperion
