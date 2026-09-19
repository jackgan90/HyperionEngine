#pragma once
#include "Hyperion/Math/Ray.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Hyperion
{
struct FBoundsQueryStats
{
	std::size_t Groups{};
	std::size_t UnboundedGroups{};
	std::size_t VisitedNodes{};
	std::size_t GroupTests{};
	std::size_t CandidateGroups{};
	std::size_t IndexRebuilds{};
	std::size_t IndexRefits{};
	double UpdateMilliseconds{};
	double QueryMilliseconds{};
};

class IBoundsVisibility
{
public:
	virtual ~IBoundsVisibility() = default;
	virtual bool Intersects(const FBounds& InBounds) const = 0;
};

// Single-owner mutable object index. Ray visitors may reduce the accepted maximum distance.
class IBoundsSpatialIndex
{
public:
	virtual ~IBoundsSpatialIndex() = default;
	virtual void Set(std::uint64_t InId, FBounds InBounds) = 0;
	virtual void Remove(std::uint64_t InId) = 0;
	virtual void Commit(FBoundsQueryStats& OutStats) = 0;
	virtual std::vector<std::uint64_t> Query(const IBoundsVisibility* InVisibility, bool bInHierarchy,
	                                         FBoundsQueryStats& OutStats) const = 0;
	virtual void Raycast(FRay InRay, const std::function<void(std::uint64_t, float&)>& InVisit,
	                     FBoundsQueryStats& OutStats) const = 0;
};

std::unique_ptr<IBoundsSpatialIndex> CreateBoundsBvh();
} // namespace Hyperion
