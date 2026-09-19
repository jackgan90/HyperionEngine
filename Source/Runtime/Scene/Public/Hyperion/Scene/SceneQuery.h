#pragma once
#include "Hyperion/Math/BoundsBvh.h"
#include "Hyperion/Math/StaticBoundsBvh.h"
#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{
struct FSceneModelGeometry
{
	std::vector<FStaticBoundsBvh> Primitives;
	std::size_t StorageBytes{};
};

// Pure CPU preparation. Call on Worker before publishing the containing model data.
std::shared_ptr<const FSceneModelGeometry> PrepareSceneModelGeometry(
    const FModelAsset& InAsset, const std::function<void()>& InCheckCancellation = {});

enum class ESceneRayStatus
{
	Hit,
	Miss,
	Unavailable
};

struct FSceneRayOptions
{
	bool bTwoSided{};
	std::vector<std::string> MaterialUsages{"Forward"};
	// A usage is ineligible when the material defines any of its excluded usages.
	std::map<std::string, std::vector<std::string>> MaterialUsageExclusions;
};

struct FSceneRayStats
{
	FBoundsQueryStats Bounds;
	std::size_t InstanceTests{};
	std::size_t TriangleNodes{};
	std::size_t TriangleTests{};
	std::size_t UnavailableCandidates{};
};

struct FSceneRayResult
{
	ESceneRayStatus Status = ESceneRayStatus::Unavailable;
	FSceneHandle Handle;
	std::uint32_t Instance{};
	std::uint32_t Primitive{};
	std::uint32_t Triangle{};
	float Distance{};
	FVec3 Position;
	FVec3 Barycentrics;
	// A hit is the nearest prepared geometry; unloaded candidates may still be present.
	bool bIncomplete{};
	FSceneRayStats Stats;
};
} // namespace Hyperion
