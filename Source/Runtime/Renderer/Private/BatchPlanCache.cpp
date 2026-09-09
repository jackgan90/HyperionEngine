#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "RenderBatchInternal.h"
#include <algorithm>

namespace Hyperion
{
bool FRenderBatchSystem::FImpl::FPlanItem::Matches(const FRenderItem& InItem, bool bInSharedRefresh) const
{
	if (!InItem.PreparationError.empty() || InItem.Primitive != Primitive || InItem.LocalItemId != LocalId ||
	    !InItem.Lifetime || Lifetime.lock() != InItem.Lifetime || Resource.lock() != InItem.State.Resource ||
	    Surface.lock() != InItem.State.Surface || Section != InItem.State.Section ||
	    bMirrored != (Determinant(InItem.State.World) < 0) || Dynamic != InItem.DynamicState)
	{
		return false;
	}
	if (Values.lock() == InItem.ResolvedParameters)
	{
		return true;
	}
	if (!bInSharedRefresh || !InItem.ResolvedParameters || !InItem.ResolvedParameters->ResourceIdentity ||
	    Resources.lock() != InItem.ResolvedParameters->ResourceIdentity ||
	    !ParameterValues.SharesLocalValues(InItem.ResolvedParameters->Values))
	{
		return false;
	}
	for (const auto Index : InstanceParameters)
	{
		if (!SameMaterialValue(ParameterValues[Index], InItem.ResolvedParameters->Values[Index]))
		{
			return false;
		}
	}
	return true;
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
	if (Entry.Depth != InSnapshot.DepthFormat || Entry.Inputs.size() != InSnapshot.Items.size() ||
	    !std::equal(Entry.Inputs.begin(), Entry.Inputs.end(), InSnapshot.Items.begin(),
	                [this](const auto& InCached, const auto& InItem)
	                {
		                return InCached.Matches(InItem, Strategies.size() == 1);
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
			const bool bChanged = std::any_of(Members.begin(), Members.end(),
			                                  [&](const auto InIndex)
			                                  {
				                                  return Entry.Inputs[InIndex].Values.lock() !=
				                                         InSnapshot.Items[InIndex].ResolvedParameters;
			                                  });
			if (bChanged)
			{
				const auto Shared = InSnapshot.Items[Members.front()].ResolvedParameters->Values.GetSharedIdentity();
				// Local values and resources match the old proof. Identical overlays preserve equality within
				// this group, including shared constants; differing override masks require full regrouping.
				if (!Shared ||
				    !std::all_of(Members.begin(), Members.end(),
				                 [&](const auto InIndex)
				                 {
					                 return InSnapshot.Items[InIndex].ResolvedParameters->Values.GetSharedIdentity() ==
					                        Shared;
				                 }))
				{
					return {};
				}
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
	for (std::size_t Index = 0; Index < Entry.Inputs.size(); ++Index)
	{
		Entry.Inputs[Index].Values = InSnapshot.Items[Index].ResolvedParameters;
	}
	Result->Statistics.PlanReuses = 1;
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
		if (!Item.LocalItemId || !Item.Lifetime || !Item.State.Surface || !Item.State.Resource ||
		    !Item.ResolvedParameters || !Item.PreparationError.empty())
		{
			return;
		}
		FPlanItem Input{Item.Primitive,
		                *Item.LocalItemId,
		                Item.Lifetime,
		                Item.State.Resource,
		                Item.State.Surface,
		                Item.ResolvedParameters,
		                Item.ResolvedParameters->Values,
		                Item.ResolvedParameters->ResourceIdentity};
		Input.Section = Item.State.Section;
		Input.bMirrored = Determinant(Item.State.World) < 0;
		Input.Dynamic = Item.DynamicState;
		const auto Program = Item.State.Surface->GetCompiled();
		if (const auto* Pass = Program->FindInstancePass(InSnapshot.View.Usage))
		{
			for (const auto& Binding : Pass->Bindings)
			{
				if (Binding.InstanceStride)
				{
					for (const auto& Member : Binding.Members)
					{
						Input.InstanceParameters.push_back(Member.ParameterIndex);
					}
				}
			}
		}
		Entry.Inputs.push_back(std::move(Input));
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
