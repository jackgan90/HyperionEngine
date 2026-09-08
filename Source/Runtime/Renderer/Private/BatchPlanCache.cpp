#include "RenderBatchInternal.h"
#include <algorithm>

namespace Hyperion
{
bool FRenderBatchSystem::FImpl::FPlanItem::Matches(const FRenderItem& InItem) const
{
	return InItem.PreparationError.empty() && InItem.Primitive == Primitive && InItem.LocalItemId == LocalId &&
	       InItem.Lifetime && Lifetime.lock() == InItem.Lifetime && Resource.lock() == InItem.State.Resource &&
	       Surface.lock() == InItem.State.Surface && Values.lock() == InItem.ResolvedParameters &&
	       Section == InItem.State.Section && bMirrored == (Determinant(InItem.State.World) < 0) &&
	       Dynamic == InItem.DynamicState;
}

std::shared_ptr<FRenderBatchPlan> FRenderBatchSystem::FImpl::ReusePlan(const FRenderSceneSnapshot& InSnapshot)
{
	const auto Existing = Plans.find({InSnapshot.View.Identity, InSnapshot.View.Usage});
	if (Existing == Plans.end())
	{
		return {};
	}
	auto& Entry = Existing->second;
	if (Entry.Depth != InSnapshot.DepthFormat || Entry.Inputs.size() != InSnapshot.Items.size() ||
	    !std::equal(Entry.Inputs.begin(), Entry.Inputs.end(), InSnapshot.Items.begin(),
	                [](const auto& InCached, const auto& InItem)
	                {
		                return InCached.Matches(InItem);
	                }))
	{
		return {};
	}
	auto Result = std::make_shared<FRenderBatchPlan>();
	Result->Statistics.EligibleItems = Entry.Statistics.EligibleItems;
	Result->Statistics.CapacitySplits = Entry.Statistics.CapacitySplits;
	Result->Statistics.Fallbacks = Entry.Statistics.Fallbacks;
	Result->Statistics.CompatibilityReuses = Entry.Inputs.size();
	for (const auto& Members : Entry.Members)
	{
		FRenderBatch Batch{Members};
		if (Members.size() > 1)
		{
			const auto Key = BatchItemKey(InSnapshot, InSnapshot.Items[Members.front()]);
			const auto Chunk = Chunks.find(Key);
			if (Chunk == Chunks.end() || !Chunk->second.Data->IsLive())
			{
				return {};
			}
			Batch.Instances = Chunk->second.Data;
			Chunk->second.Access = Access;
			RecentChunks.splice(RecentChunks.end(), RecentChunks, Chunk->second.Recent);
			++Result->Statistics.ReusedChunks;
		}
		Result->Batches.push_back(std::move(Batch));
	}
	Entry.Access = Access;
	return Result;
}

void FRenderBatchSystem::FImpl::CachePlan(const FRenderSceneSnapshot& InSnapshot, const FRenderBatchPlan& InPlan)
{
	const auto Key = std::pair{InSnapshot.View.Identity, InSnapshot.View.Usage};
	const auto Existing = Plans.find(Key);
	if (Existing != Plans.end())
	{
		PlanItems -= Existing->second.Inputs.size();
		Plans.erase(Existing);
	}
	if (InSnapshot.Items.empty() || InSnapshot.Items.size() > Limits.MaxItems)
	{
		return;
	}
	FPlanEntry Entry;
	Entry.Depth = InSnapshot.DepthFormat;
	Entry.Access = Access;
	Entry.Statistics = InPlan.Statistics;
	for (const auto& Item : InSnapshot.Items)
	{
		if (!Item.LocalItemId || !Item.Lifetime || !Item.ResolvedParameters || !Item.PreparationError.empty())
		{
			return;
		}
		Entry.Inputs.push_back({Item.Primitive, *Item.LocalItemId, Item.Lifetime, Item.State.Resource,
		                        Item.State.Surface, Item.ResolvedParameters, Item.State.Section,
		                        Determinant(Item.State.World) < 0, Item.DynamicState});
	}
	for (const auto& Batch : InPlan.Batches)
	{
		if (Batch.Items.size() > 1 && !Batch.Instances)
		{
			return;
		}
		Entry.Members.push_back(Batch.Items);
	}
	while (!Plans.empty() && (PlanItems + Entry.Inputs.size() > Limits.MaxItems || Plans.size() >= 16))
	{
		const auto Oldest = std::min_element(Plans.begin(), Plans.end(),
		                                     [](const auto& InA, const auto& InB)
		                                     {
			                                     return InA.second.Access < InB.second.Access;
		                                     });
		PlanItems -= Oldest->second.Inputs.size();
		Plans.erase(Oldest);
	}
	PlanItems += Entry.Inputs.size();
	Plans.emplace(Key, std::move(Entry));
}
} // namespace Hyperion
