#pragma once
#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{
struct FLightPriorityCandidate
{
	FSceneHandle Handle;
	std::string Id;
	std::int32_t Priority{};
};

struct FLightPrioritySelection
{
	std::optional<FSceneHandle> Handle;
	bool bTied{};
};

struct FSceneLightingSelection
{
	FLightPrioritySelection Directional;
	FLightPrioritySelection Environment;
};

FLightPrioritySelection ResolveLightPriority(std::span<const FLightPriorityCandidate> InCandidates);
bool CanCastDirectionalShadows(const FSceneDirectionalLight& InLight);
} // namespace Hyperion
