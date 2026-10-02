#pragma once
#include "Hyperion/RasterOptions/RasterOptions.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Hyperion
{
enum class ESceneMaterialRoute
{
	DeferredBase,
	ForwardOpaque,
	Compatibility,
	Transparent,
	Legacy
};

// Built-in scene routing facts only; material Usage remains an open string selector.
struct FSceneMaterialRoute
{
	ESceneMaterialRoute Id;
	std::string_view Usage;
	bool bDeferred;
	bool bForward;
	bool bPickable;
	bool bExcludesLegacy;

	bool Supports(ESceneRenderPipeline InPipeline) const;
};

std::span<const FSceneMaterialRoute> SceneMaterialRoutes();
const FSceneMaterialRoute& DescribeSceneMaterialRoute(ESceneMaterialRoute InId);
std::vector<std::string> ScenePickMaterialUsages(ESceneRenderPipeline InPipeline);
std::vector<std::string> SceneLegacyPassExclusions();
} // namespace Hyperion
