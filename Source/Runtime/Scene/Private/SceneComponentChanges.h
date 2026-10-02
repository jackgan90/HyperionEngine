#pragma once
#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{
struct FSceneComponentDifference
{
	std::vector<FSceneComponentChange> Changes;
	ESceneChangeMask Effects = ESceneChangeMask::None;
};

FSceneComponentDifference BuildSceneComponentDifference(const FSceneNode* InBefore, const FSceneNode* InAfter,
                                                        bool bInInitialSync);
std::vector<FSceneComponentChange> MergeSceneComponentChanges(std::span<const FSceneComponentChange> InBefore,
                                                              std::span<const FSceneComponentChange> InAfter);
} // namespace Hyperion
