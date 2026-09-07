#include "Hyperion/Renderer/SceneSpatialIndex.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>

namespace Hyperion
{
namespace
{
class FSceneBvh final : public ISceneSpatialIndex
{
public:
	void Set(std::uint64_t InId, FBounds InBounds) override;
	void Remove(std::uint64_t InId) override;
	void Commit(FSceneVisibilityStats& OutStats) override;
	std::vector<std::uint64_t> Query(const ISceneVisibility* InVisibility, bool bInHierarchy,
	                                 FSceneVisibilityStats& OutStats) const override;

private:
	struct FNode
	{
		FBounds Bounds;
		std::uint64_t Id{};
		int Parent = -1;
		int Left = -1;
		int Right = -1;
	};

	int Build(std::vector<std::uint64_t>& InIds, std::size_t InBegin, std::size_t InEnd, int InParent);
	void Rebuild();
	void Refit(std::uint64_t InId);
	std::map<std::uint64_t, FBounds> Bounds;
	std::set<std::uint64_t> Unbounded;
	std::set<std::uint64_t> Dirty;
	std::map<std::uint64_t, int> Leaves;
	std::vector<FNode> Nodes;
	bool bTopologyDirty{};
	double AreaSum{};
	double BaselineCost{};
};

bool EqualBounds(const FBounds& InA, const FBounds& InB)
{
	return InA.bValid == InB.bValid && InA.Minimum.X == InB.Minimum.X && InA.Minimum.Y == InB.Minimum.Y &&
	       InA.Minimum.Z == InB.Minimum.Z && InA.Maximum.X == InB.Maximum.X && InA.Maximum.Y == InB.Maximum.Y &&
	       InA.Maximum.Z == InB.Maximum.Z;
}

void FSceneBvh::Set(std::uint64_t InId, FBounds InBounds)
{
	const bool bKnown = IsUsable(InBounds);
	if (!bKnown)
	{
		InBounds = {};
	}
	const auto It = Bounds.find(InId);
	if (It != Bounds.end() && EqualBounds(It->second, InBounds))
	{
		return;
	}
	bTopologyDirty |= It == Bounds.end() || IsUsable(It->second) != bKnown;
	Bounds.insert_or_assign(InId, InBounds);
	if (bKnown)
	{
		Unbounded.erase(InId);
		Dirty.insert(InId);
	}
	else
	{
		Unbounded.insert(InId);
	}
}

void FSceneBvh::Remove(std::uint64_t InId)
{
	bTopologyDirty |= Bounds.erase(InId) != 0;
	Unbounded.erase(InId);
	Dirty.erase(InId);
}

int FSceneBvh::Build(std::vector<std::uint64_t>& InIds, std::size_t InBegin, std::size_t InEnd, int InParent)
{
	const auto Index = static_cast<int>(Nodes.size());
	Nodes.push_back({Bounds.at(InIds[InBegin]), 0, InParent});
	if (InEnd - InBegin == 1)
	{
		Nodes[Index].Id = InIds[InBegin];
		Leaves.emplace(InIds[InBegin], Index);
		return Index;
	}
	for (auto Item = InBegin + 1; Item < InEnd; ++Item)
	{
		Nodes[Index].Bounds = UnionBounds(Nodes[Index].Bounds, Bounds.at(InIds[Item]));
	}
	const auto Extent = Subtract(Nodes[Index].Bounds.Maximum, Nodes[Index].Bounds.Minimum);
	const unsigned Axis = Extent.X >= Extent.Y && Extent.X >= Extent.Z ? 0 : Extent.Y >= Extent.Z ? 1 : 2;
	const auto Mid = InBegin + (InEnd - InBegin) / 2;
	std::nth_element(
	    InIds.begin() + InBegin, InIds.begin() + Mid, InIds.begin() + InEnd,
	    [this, Axis](std::uint64_t InA, std::uint64_t InB)
	    {
		    const auto& A = Bounds.at(InA);
		    const auto& B = Bounds.at(InB);
		    const std::array<double, 3> CenterA{double(A.Minimum.X) + A.Maximum.X, double(A.Minimum.Y) + A.Maximum.Y,
		                                        double(A.Minimum.Z) + A.Maximum.Z};
		    const std::array<double, 3> CenterB{double(B.Minimum.X) + B.Maximum.X, double(B.Minimum.Y) + B.Maximum.Y,
		                                        double(B.Minimum.Z) + B.Maximum.Z};
		    return CenterA[Axis] == CenterB[Axis] ? InA < InB : CenterA[Axis] < CenterB[Axis];
	    });
	const auto Left = Build(InIds, InBegin, Mid, Index);
	const auto Right = Build(InIds, Mid, InEnd, Index);
	Nodes[Index].Left = Left;
	Nodes[Index].Right = Right;
	return Index;
}

void FSceneBvh::Rebuild()
{
	std::vector<std::uint64_t> Ids;
	for (const auto& [Id, Bound] : Bounds)
	{
		if (IsUsable(Bound))
		{
			Ids.push_back(Id);
		}
	}
	Nodes.clear();
	Leaves.clear();
	Nodes.reserve(Ids.size() * 2);
	if (!Ids.empty())
	{
		Build(Ids, 0, Ids.size(), -1);
	}
	AreaSum = 0;
	for (const auto& Node : Nodes)
	{
		AreaSum += BoundsArea(Node.Bounds);
	}
	BaselineCost = Nodes.empty() ? 0 : AreaSum / std::max(.0001, double(BoundsArea(Nodes[0].Bounds)));
	bTopologyDirty = false;
}

void FSceneBvh::Refit(std::uint64_t InId)
{
	auto Index = Leaves.at(InId);
	while (Index >= 0)
	{
		auto& Node = Nodes[Index];
		AreaSum -= BoundsArea(Node.Bounds);
		Node.Bounds = Node.Left < 0 ? Bounds.at(InId) : UnionBounds(Nodes[Node.Left].Bounds, Nodes[Node.Right].Bounds);
		AreaSum += BoundsArea(Node.Bounds);
		Index = Node.Parent;
	}
}

void FSceneBvh::Commit(FSceneVisibilityStats& OutStats)
{
	const auto Start = std::chrono::steady_clock::now();
	if (bTopologyDirty)
	{
		Rebuild();
		++OutStats.IndexRebuilds;
	}
	else
	{
		for (const auto Id : Dirty)
		{
			Refit(Id);
			++OutStats.IndexRefits;
		}
		if (!Nodes.empty() && AreaSum / std::max(.0001, double(BoundsArea(Nodes[0].Bounds))) > BaselineCost * 2)
		{
			Rebuild();
			++OutStats.IndexRebuilds;
		}
	}
	Dirty.clear();
	OutStats.Groups = Bounds.size();
	OutStats.UnboundedGroups = Unbounded.size();
	OutStats.UpdateMilliseconds +=
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
}

std::vector<std::uint64_t> FSceneBvh::Query(const ISceneVisibility* InVisibility, bool bInHierarchy,
                                            FSceneVisibilityStats& OutStats) const
{
	const auto Start = std::chrono::steady_clock::now();
	std::vector<std::uint64_t> Result;
	if (!bInHierarchy || !InVisibility)
	{
		for (const auto& [Id, Bound] : Bounds)
		{
			OutStats.GroupTests += InVisibility && IsUsable(Bound) ? 1 : 0;
			if (!InVisibility || InVisibility->Intersects(Bound))
			{
				Result.push_back(Id);
			}
		}
	}
	else
	{
		Result.assign(Unbounded.begin(), Unbounded.end());
		std::vector<int> Stack;
		if (!Nodes.empty())
		{
			Stack.push_back(0);
		}
		while (!Stack.empty())
		{
			const auto& Node = Nodes[Stack.back()];
			Stack.pop_back();
			++OutStats.VisitedNodes;
			OutStats.GroupTests += Node.Left < 0 ? 1 : 0;
			if (!InVisibility->Intersects(Node.Bounds))
			{
				continue;
			}
			if (Node.Left < 0)
			{
				Result.push_back(Node.Id);
			}
			else
			{
				Stack.push_back(Node.Right);
				Stack.push_back(Node.Left);
			}
		}
	}
	OutStats.CandidateGroups = Result.size();
	OutStats.QueryMilliseconds +=
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	return Result;
}
} // namespace

std::unique_ptr<ISceneSpatialIndex> CreateBvhSpatialIndex()
{
	return std::make_unique<FSceneBvh>();
}
} // namespace Hyperion
