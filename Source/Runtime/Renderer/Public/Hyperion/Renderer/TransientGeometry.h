#pragma once
#include "Hyperion/Renderer/RenderFeatures.h"

namespace Hyperion
{
// Immutable, viewport-owned geometry. Never enters the scene, visibility index or shadow family.
struct FTransientGeometry
{
	std::vector<FRenderPrimitiveState> Items;
	// Source materials participate in ordinary scene passes, including transparent sorting, but never shadows.
	std::vector<FRenderPrimitiveState> SceneItems;
	// During publication, these persistent items are replaced by this snapshot in every view.
	std::vector<FRenderPrimitiveHandle> ReplacedPrimitives;
	std::shared_ptr<const void> Lifetime;
	void AddModelInstance(FRenderPrimitiveState InState, std::shared_ptr<const FRenderMaterial> InFallback);
};

void AppendTransientSceneItems(FRenderSceneSnapshot& InSnapshot, const FTransientGeometry& InGeometry);
std::unique_ptr<IRenderFeature> MakeTransientGeometryFeature();
std::shared_ptr<const FMaterialSnapshot> MakeGeometryPreviewMaterial();
} // namespace Hyperion
