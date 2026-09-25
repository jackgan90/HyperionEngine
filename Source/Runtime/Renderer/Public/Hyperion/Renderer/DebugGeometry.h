#pragma once
#include "Hyperion/Renderer/ViewportRay.h"
#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{
struct FDebugLine
{
	FVec3 A;
	FVec3 B;
	FVec4 Color{1, .8f, .3f, 1};
};

std::vector<FDebugLine> BuildSceneDebugLines(std::span<const FSceneNodeView> InNodes, bool bInModels, bool bInLights);
std::optional<std::array<FVec2, 2>> ProjectDebugLine(const FDebugLine& InLine, const FMat4& InViewProjection,
                                                     FVec4 InViewport);
} // namespace Hyperion
