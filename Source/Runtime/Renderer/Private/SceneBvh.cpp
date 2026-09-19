#include "Hyperion/Renderer/SceneSpatialIndex.h"

namespace Hyperion
{
namespace
{
class FSceneBvh final : public ISceneSpatialIndex
{
public:
	void Set(std::uint64_t InId, FBounds InBounds) override
	{
		Index->Set(InId, InBounds);
	}

	void Remove(std::uint64_t InId) override
	{
		Index->Remove(InId);
	}

	void Commit(FSceneVisibilityStats& OutStats) override
	{
		FBoundsQueryStats Stats;
		Index->Commit(Stats);
		OutStats.Groups = Stats.Groups;
		OutStats.UnboundedGroups = Stats.UnboundedGroups;
		OutStats.IndexRebuilds += Stats.IndexRebuilds;
		OutStats.IndexRefits += Stats.IndexRefits;
		OutStats.UpdateMilliseconds += Stats.UpdateMilliseconds;
	}

	std::vector<std::uint64_t> Query(const ISceneVisibility* InVisibility, bool bInHierarchy,
	                                 FSceneVisibilityStats& OutStats) const override
	{
		FBoundsQueryStats Stats;
		auto Result = Index->Query(InVisibility, bInHierarchy, Stats);
		OutStats.VisitedNodes += Stats.VisitedNodes;
		OutStats.GroupTests += Stats.GroupTests;
		OutStats.CandidateGroups = Stats.CandidateGroups;
		OutStats.QueryMilliseconds += Stats.QueryMilliseconds;
		return Result;
	}

private:
	std::unique_ptr<IBoundsSpatialIndex> Index = CreateBoundsBvh();
};
} // namespace

std::unique_ptr<ISceneSpatialIndex> CreateBvhSpatialIndex()
{
	return std::make_unique<FSceneBvh>();
}
} // namespace Hyperion
