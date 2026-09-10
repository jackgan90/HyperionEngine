#pragma once
#include "Hyperion/Renderer/RenderResources.h"

namespace Hyperion
{
// Immutable, view-independent metadata. Rebuilt with collection/resource publication revisions.
struct FSceneItemPreparation
{
	std::shared_ptr<const FCompiledMaterialDefinition> Program;
	FBounds GeometryBounds;
	FBounds WorldBounds;
	bool bRequiresConservativeBounds{};
};

bool PrepareSceneItem(FRenderItem& InItem);
bool IsPreparedSceneItemVisible(const FRenderItem& InItem, const FRenderView& InView, const FFrustum& InFrustum,
                                FSceneVisibilityStats& OutStats);
} // namespace Hyperion
