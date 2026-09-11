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

FMat4 PrimitiveClipTransform(const FRenderPrimitiveState& InState, const FMat4& InViewProjection,
                             EDepthConvention InConvention);

bool IsExcludedFromView(const FMaterialDefinition& InDefinition, const FRenderView& InView);
bool PrepareSceneItem(FRenderItem& InItem);
bool IsPreparedSceneItemVisible(const FRenderItem& InItem, const FRenderView& InView, const FFrustum& InFrustum,
                                FSceneVisibilityStats& OutStats);
} // namespace Hyperion
