#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "RenderBatchInternal.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
void FRenderBatchSystem::FImpl::AdmitIncremental(const FRenderSceneSnapshot& InSnapshot,
                                                 FIncrementalBatchHistory& InHistory,
                                                 std::span<const std::size_t> InAdded,
                                                 std::vector<FRenderBatchSignature>& InSignatures,
                                                 FRenderBatchStats& OutStats)
{
	HYP_PERF_SCOPE_C(Detail, AdmitIncrementalBatchItems);
	FSharedBatchValueCache Shared;
	for (const auto Index : InAdded)
	{
		const auto& Item = InSnapshot.Items[Index];
		CurrentInputs[Index] = PrepareInput(InSnapshot, Item, OutStats);
		const bool bSrgb = Item.State.Surface->GetSnapshot()->Definition->GetPass(InSnapshot.View.Usage).bSrgbTarget;
		const auto Candidate = Describe(InSnapshot, Item, InSnapshot.Targets.GraphicsTarget(bSrgb), OutStats, Shared);
		const auto Decision = Strategies.front()->Evaluate(*Candidate, Capabilities);
		if (!Candidate->bReorderable || Decision.Capacity < 2 ||
		    !InHistory.Insert(InSnapshot, Index, Candidate->Signature, Decision.Capacity, Limits.MaxChunks,
		                      InSignatures))
		{
			throw std::runtime_error("Incremental batch admission requires the complete planning path");
		}
	}
}

std::shared_ptr<FRenderBatchPlan> FRenderBatchSystem::FImpl::PublishIncremental(const FRenderSceneSnapshot& InSnapshot,
                                                                                FIncrementalBatchHistory& InHistory,
                                                                                FRenderBatchStats InStats)
{
	HYP_PERF_SCOPE_C(Detail, PublishIncrementalBatches);
	const bool bHadStructure = bool(InHistory.Structure);
	auto Result = std::make_shared<FRenderBatchPlan>();
	Result->Statistics = InStats;
	Result->Statistics.EligibleItems = InSnapshot.Items.Size();
	Result->Batches.reserve(InHistory.Blocks.size());
	InHistory.RefreshIndices();
	for (auto& Block : InHistory.Blocks)
	{
		if (Block.Members.empty())
		{
			continue;
		}
		FRenderBatch Batch;
		Batch.Items = Block.Indices;
		if (Block.bDirty)
		{
			Block.Contents = std::make_shared<const int>(0);
			++Result->Statistics.AffectedBatches;
		}
		else
		{
			++Result->Statistics.RetainedBatches;
		}
		Batch.LocalContentIdentity = Block.Contents;
		if (Batch.Items.size() > 1)
		{
			if (!Block.bDirty && Block.Chunk)
			{
				const auto& [Scene, Slot, Generation, LocalId] = *Block.Chunk;
				const FBatchItemKey Key{
				    InSnapshot.View.Identity, InSnapshot.View.Usage, Scene, Slot, Generation, LocalId};
				if (const auto Found = Chunks.find(Key); Found != Chunks.end() && Found->second.Data->IsLive())
				{
					Batch.Instances = Found->second.Data;
					Found->second.Access = Access;
					RecentChunks.splice(RecentChunks.end(), RecentChunks, Found->second.Recent);
					++Result->Statistics.ReusedChunks;
				}
			}
			if (!Batch.Instances)
			{
				for (const auto Index : Batch.Items)
				{
					if (!CurrentInputs[Index])
					{
						CurrentInputs[Index] = PrepareInput(InSnapshot, InSnapshot.Items[Index], Result->Statistics);
					}
				}
				Batch.Instances = Data(InSnapshot, Batch, Result->Statistics);
				Block.Chunk = Block.Members.front();
			}
		}
		else
		{
			++Result->Statistics.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Singleton)];
		}
		Block.bDirty = false;
		Result->Batches.push_back(std::move(Batch));
	}
	if (InHistory.bStructureChanged)
	{
		InHistory.Structure = std::make_shared<const int>(0);
	}
	// Count the current retained capacity layout, including unchanged boundaries and excluding empty groups.
	Result->Statistics.CapacitySplits =
	    Result->Batches.size() - std::count_if(InHistory.Groups.begin(), InHistory.Groups.end(),
	                                           [](const auto& InGroup)
	                                           {
		                                           return bool(InGroup.Structure);
	                                           });
	Result->StructureIdentity = InHistory.Structure;
	Result->Statistics.PlanReuses = !InHistory.bStructureChanged;
	Result->Statistics.LocalPlanReuses =
	    !InHistory.bStructureChanged && InHistory.LocalContents.lock() == InSnapshot.LocalContentIdentity;
	Result->Statistics.IncrementalPlanUpdates = InHistory.bStructureChanged && bHadStructure;
	InHistory.LocalContents = InSnapshot.LocalContentIdentity;
	return Result;
}

void FRenderBatchSystem::FImpl::LimitIncrementalHistory()
{
	std::size_t ItemCount{};
	std::size_t BlockCount{};
	for (const auto& [Key, History] : IncrementalPlans)
	{
		ItemCount += History.Sources.size();
		BlockCount += History.Blocks.size();
	}
	while (!IncrementalPlans.empty() &&
	       (ItemCount > Limits.MaxItems || BlockCount > Limits.MaxChunks || IncrementalPlans.size() > 16))
	{
		const auto Oldest = std::min_element(IncrementalPlans.begin(), IncrementalPlans.end(),
		                                     [](const auto& InA, const auto& InB)
		                                     {
			                                     return InA.second.Access < InB.second.Access;
		                                     });
		ItemCount -= Oldest->second.Sources.size();
		BlockCount -= Oldest->second.Blocks.size();
		IncrementalPlans.erase(Oldest);
	}
}

std::shared_ptr<FRenderBatchPlan> FRenderBatchSystem::FImpl::BuildIncremental(const FRenderSceneSnapshot& InSnapshot)
{
	HYP_PERF_SCOPE_C(Detail, BuildIncrementalBatchPlan);
	const auto Key = std::pair{InSnapshot.View.Identity, InSnapshot.View.Usage};
	if (Strategies.size() != 1 || !InSnapshot.LocalContentIdentity || !InSnapshot.CollectionKey[0] ||
	    !InSnapshot.CollectionKey[1] || !InSnapshot.CollectionKey[2] || InSnapshot.Items.IsEmpty() ||
	    InSnapshot.Items.Size() > Limits.MaxItems || !Limits.MaxChunks)
	{
		IncrementalPlans.erase(Key);
		return {};
	}
	// The complete cache addresses chunks by their first item. Drop its old layout before changing any chunks.
	if (const auto Old = Plans.find(Key); Old != Plans.end())
	{
		PlanItems -= Old->second.Inputs.size();
		Plans.erase(Old);
	}
	try
	{
		auto& History = IncrementalPlans[Key];
		if (History.Scene != InSnapshot.CollectionKey[0] || History.ResourceRevision != InSnapshot.CollectionKey[2] ||
		    History.Target != InSnapshot.Targets.GraphicsTarget() ||
		    History.Depth != InSnapshot.Targets.GetDepthFormat() ||
		    History.ColorCount != InSnapshot.Targets.ColorCount())
		{
			History = {};
			History.Scene = InSnapshot.CollectionKey[0];
			History.ResourceRevision = InSnapshot.CollectionKey[2];
			History.Depth = InSnapshot.Targets.GetDepthFormat();
			History.ColorCount = InSnapshot.Targets.ColorCount();
			History.Target = InSnapshot.Targets.GraphicsTarget();
		}
		FRenderBatchStats Statistics;
		std::vector<std::size_t> Added;
		if (!History.UpdateMembership(InSnapshot, Access, Added, Statistics))
		{
			IncrementalPlans.erase(Key);
			return {};
		}
		History.RefreshIndices();
		// Current numeric values belong to this build only. Persistent groups retain no programs or source values.
		std::vector<FRenderBatchSignature> Signatures;
		if (!History.RefreshGroups(InSnapshot, Signatures))
		{
			IncrementalPlans.erase(Key);
			return {};
		}
		CurrentInputs.clear();
		CurrentInputs.resize(InSnapshot.Items.Size());
		AdmitIncremental(InSnapshot, History, Added, Signatures, Statistics);
		auto Result = PublishIncremental(InSnapshot, History, Statistics);
		LimitIncrementalHistory();
		return Result;
	}
	catch (const std::exception&)
	{
		// No published plan was mutated. Discard partial metadata before running the full transaction.
		IncrementalPlans.erase(Key);
		return {};
	}
}
} // namespace Hyperion
