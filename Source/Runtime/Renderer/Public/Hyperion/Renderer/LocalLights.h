#pragma once
#include "Hyperion/Renderer/ScenePublication.h"
#include "Hyperion/Renderer/SceneSpatialIndex.h"
#include <map>

namespace Hyperion
{
struct FLocalLight
{
	FSceneHandle Handle;
	FVec3 Position;
	FVec3 Direction{0, 0, -1};
	FVec3 Radiance;
	float Range{};
	float InnerCos = 1;
	float OuterCos = -1;
	bool bSpot{};
	FMat4 VolumeWorld = Identity();
	FBounds Bounds;
};

struct FLocalLightStatistics
{
	std::size_t Points{};
	std::size_t Spots{};
	std::size_t VisiblePoints{};
	std::size_t VisibleSpots{};
	std::size_t Draws{};
	bool bActive{};
	FSceneVisibilityStats Spatial;
};

// Render-owned index, independent of geometry and shadow caster membership.
class FLocalLightIndex
{
public:
	std::vector<FLocalLight> Query(const FSceneMetadata& InMetadata, const ISceneVisibility* InVisibility,
	                               bool bInHierarchy, FLocalLightStatistics& OutStatistics);

private:
	std::optional<FScenePublicationToken> Token;
	std::uint64_t LocalRevision{};
	std::map<std::uint64_t, FLocalLight> Lights;
	std::unique_ptr<ISceneSpatialIndex> Spatial = CreateBvhSpatialIndex();
	void Update(const FSceneMetadata& InMetadata, FSceneVisibilityStats& OutStatistics);
};
} // namespace Hyperion
