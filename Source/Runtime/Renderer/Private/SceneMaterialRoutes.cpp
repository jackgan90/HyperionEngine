#include "SceneMaterialRoutes.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace Hyperion
{
namespace
{
// Filtering preserves both existing ordered pick lists and the complete HDR exclusion order.
constexpr std::array Routes{
    FSceneMaterialRoute{ESceneMaterialRoute::ForwardOpaque, "HdrForwardOpaque", false, true, true, true},
    FSceneMaterialRoute{ESceneMaterialRoute::DeferredBase, "DeferredBase", true, false, true, true},
    FSceneMaterialRoute{ESceneMaterialRoute::Compatibility, "HdrCompatibility", true, false, true, true},
    FSceneMaterialRoute{ESceneMaterialRoute::Transparent, "HdrTransparent", true, true, true, true},
    FSceneMaterialRoute{ESceneMaterialRoute::Legacy, "Forward", true, true, true, false}};
} // namespace

bool FSceneMaterialRoute::Supports(ESceneRenderPipeline InPipeline) const
{
	return InPipeline == ESceneRenderPipeline::Deferred ? bDeferred : bForward;
}

std::span<const FSceneMaterialRoute> SceneMaterialRoutes()
{
	return Routes;
}

const FSceneMaterialRoute& DescribeSceneMaterialRoute(ESceneMaterialRoute InId)
{
	const auto Found = std::find_if(Routes.begin(), Routes.end(),
	                                [InId](const auto& InRoute)
	                                {
		                                return InRoute.Id == InId;
	                                });
	if (Found == Routes.end())
	{
		throw std::invalid_argument("Unknown built-in scene material route");
	}
	return *Found;
}

std::vector<std::string> ScenePickMaterialUsages(ESceneRenderPipeline InPipeline)
{
	std::vector<std::string> Result;
	Result.reserve(Routes.size());
	for (const auto& Route : SceneMaterialRoutes())
	{
		if (Route.bPickable && Route.Supports(InPipeline))
		{
			Result.emplace_back(Route.Usage);
		}
	}
	return Result;
}

std::vector<std::string> SceneLegacyPassExclusions()
{
	std::vector<std::string> Result;
	Result.reserve(Routes.size());
	for (const auto& Route : SceneMaterialRoutes())
	{
		if (Route.bExcludesLegacy)
		{
			Result.emplace_back(Route.Usage);
		}
	}
	return Result;
}
} // namespace Hyperion
