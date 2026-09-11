#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "RenderBatchInternal.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
bool SameSharedParameters(const FRenderSceneSnapshot& InSnapshot, std::span<const std::size_t> InMembers,
                          bool bInAllowEmpty)
{
	const auto SharedIdentity = [](const FRenderItem& InItem) -> const void*
	{
		return InItem.SharedParameters ? static_cast<const void*>(InItem.SharedParameters.get())
		                               : InItem.ResolvedParameters->Values.GetSharedIdentity();
	};
	const auto Shared = SharedIdentity(InSnapshot.Items[InMembers.front()]);
	// Local values and resources match the old proof. Identical overlays preserve equality within
	// this group, including shared constants; differing override masks require full regrouping.
	return (Shared || bInAllowEmpty) && std::all_of(InMembers.begin(), InMembers.end(),
	                                                [&](const auto InIndex)
	                                                {
		                                                return SharedIdentity(InSnapshot.Items[InIndex]) == Shared;
	                                                });
}
} // namespace

bool FRenderBatchSystem::FImpl::FPlanItem::Matches(const FRenderItem& InItem, bool bInSharedRefresh) const
{
	const auto* PreparedInput = Input.Get();
	if (!PreparedInput || !PreparedInput->MatchesState(InItem))
	{
		return false;
	}
	if (Values.lock() == InItem.ResolvedParameters)
	{
		if (SameBatchOwner(Shared, InItem.SharedParameters))
		{
			return true;
		}
		return bInSharedRefresh && PreparedInput->MatchesInstances(InItem);
	}
	return bInSharedRefresh && PreparedInput->MatchesValues(InItem);
}

std::shared_ptr<FRenderBatchPlan> FRenderBatchSystem::FImpl::ReusePlan(const FRenderSceneSnapshot& InSnapshot)
{
	HYP_PERF_SCOPE_C(Detail, ReuseBatchPlan);
	const auto Existing = Plans.find({InSnapshot.View.Identity, InSnapshot.View.Usage});
	if (Existing == Plans.end())
	{
		return {};
	}
	auto& Entry = Existing->second;
	const bool bLocalProof = Strategies.size() == 1 && InSnapshot.LocalContentIdentity &&
	                         Entry.LocalContents.lock() == InSnapshot.LocalContentIdentity;
	if (Entry.Target != InSnapshot.Targets.GraphicsTarget() || Entry.Depth != InSnapshot.Targets.GetDepthFormat() ||
	    Entry.ColorCount != InSnapshot.Targets.ColorCount() || Entry.Inputs.size() != InSnapshot.Items.Size() ||
	    (!bLocalProof && !std::equal(Entry.Inputs.begin(), Entry.Inputs.end(), InSnapshot.Items.begin(),
	                                 [this](const auto& InCached, const auto& InItem)
	                                 {
		                                 return InCached.Matches(InItem, Strategies.size() == 1);
	                                 })))
	{
		return {};
	}
	auto Result = std::make_shared<FRenderBatchPlan>();
	Result->StructureIdentity = Entry.StructureIdentity;
	Result->Statistics.EligibleItems = Entry.Statistics.EligibleItems;
	Result->Statistics.CapacitySplits = Entry.Statistics.CapacitySplits;
	Result->Statistics.Fallbacks = Entry.Statistics.Fallbacks;
	Result->Statistics.CompatibilityReuses = Entry.Inputs.size();
	for (const auto& Members : Entry.Members)
	{
		FRenderBatch Batch{Members};
		if (Members.size() > 1)
		{
			const bool bChanged =
			    bLocalProof || std::any_of(Members.begin(), Members.end(),
			                               [&](const auto InIndex)
			                               {
				                               return Entry.Inputs[InIndex].Values.lock() !=
				                                          InSnapshot.Items[InIndex].ResolvedParameters ||
				                                      !SameBatchOwner(Entry.Inputs[InIndex].Shared,
				                                                      InSnapshot.Items[InIndex].SharedParameters);
			                               });
			if (bChanged && !SameSharedParameters(InSnapshot, Members, bLocalProof))
			{
				return {};
			}
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
	for (std::size_t Index = 0; !bLocalProof && Index < Entry.Inputs.size(); ++Index)
	{
		Entry.Inputs[Index].Values = InSnapshot.Items[Index].ResolvedParameters;
		Entry.Inputs[Index].Shared = InSnapshot.Items[Index].SharedParameters;
	}
	Result->Statistics.PlanReuses = 1;
	Result->Statistics.LocalPlanReuses = bLocalProof;
	Entry.LocalContents = InSnapshot.LocalContentIdentity;
	Entry.Access = Access;
	return Result;
}

void FRenderBatchSystem::FImpl::CachePlan(const FRenderSceneSnapshot& InSnapshot, const FRenderBatchPlan& InPlan)
{
	HYP_PERF_SCOPE_C(Detail, CacheBatchPlan);
	const auto Key = std::pair{InSnapshot.View.Identity, InSnapshot.View.Usage};
	const auto Existing = Plans.find(Key);
	if (Existing != Plans.end())
	{
		PlanItems -= Existing->second.Inputs.size();
		Plans.erase(Existing);
	}
	if (InSnapshot.Items.IsEmpty() || InSnapshot.Items.Size() > Limits.MaxItems)
	{
		return;
	}
	FPlanEntry Entry;
	Entry.Depth = InSnapshot.Targets.GetDepthFormat();
	Entry.ColorCount = InSnapshot.Targets.ColorCount();
	Entry.Target = InSnapshot.Targets.GraphicsTarget();
	Entry.Access = Access;
	Entry.Statistics = InPlan.Statistics;
	Entry.LocalContents = InSnapshot.LocalContentIdentity;
	Entry.StructureIdentity = InPlan.StructureIdentity;
	Entry.Inputs.reserve(CurrentInputs.size());
	for (std::size_t Index = 0; Index < CurrentInputs.size(); ++Index)
	{
		const auto& Item = InSnapshot.Items[Index];
		const auto& Input = CurrentInputs[Index];
		if (!Input || !Item.LocalItemId || !Item.Lifetime)
		{
			return;
		}
		Entry.Inputs.push_back({Input, Item.ResolvedParameters, Item.SharedParameters});
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
