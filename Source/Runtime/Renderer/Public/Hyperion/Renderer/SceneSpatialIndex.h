#pragma once
#include "Hyperion/Math/Bounds.h"
#include "Hyperion/Renderer/RenderBatchStats.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace Hyperion
{
enum class ESceneCullingMode
{
	None,
	Linear,
	Bvh
};

struct FSceneVisibilityStats
{
	std::size_t Groups{};
	std::size_t UnboundedGroups{};
	std::size_t Primitives{};
	std::size_t VisitedNodes{};
	std::size_t GroupTests{};
	std::size_t CandidateGroups{};
	std::size_t CandidatePrimitives{};
	std::size_t CollectedPrimitives{};
	std::size_t EmittedItems{};
	std::size_t VisibleItems{};
	std::size_t Draws{};
	std::size_t IndexRebuilds{};
	std::size_t IndexRefits{};
	double UpdateMilliseconds{};
	double QueryMilliseconds{};
	FRenderBatchStats Batches;
};

// CPU conservative candidate test. Future GPU stages are separate RenderGraph work.
class ISceneVisibility
{
public:
	virtual ~ISceneVisibility() = default;
	virtual bool Intersects(const FBounds& InBounds) const = 0;
};

class FFrustumVisibility final : public ISceneVisibility
{
public:
	explicit FFrustumVisibility(const FMat4& InViewProjection) : Frustum(InViewProjection)
	{
	}

	bool Intersects(const FBounds& InBounds) const override
	{
		return Frustum.Intersects(InBounds);
	}

private:
	FFrustum Frustum;
};

// Owned and called by Render. IDs have no relationship to private tree nodes.
class ISceneSpatialIndex
{
public:
	virtual ~ISceneSpatialIndex() = default;
	virtual void Set(std::uint64_t InId, FBounds InBounds) = 0;
	virtual void Remove(std::uint64_t InId) = 0;
	virtual void Commit(FSceneVisibilityStats& OutStats) = 0;
	virtual std::vector<std::uint64_t> Query(const ISceneVisibility* InVisibility, bool bInHierarchy,
	                                         FSceneVisibilityStats& OutStats) const = 0;
};

std::unique_ptr<ISceneSpatialIndex> CreateBvhSpatialIndex();
} // namespace Hyperion
