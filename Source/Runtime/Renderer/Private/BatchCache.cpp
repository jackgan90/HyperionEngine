#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/MaterialInputValues.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "RenderBatchInternal.h"
#include <algorithm>

namespace Hyperion
{
std::shared_ptr<const FRenderBatchValues> FSharedBatchValueCache::Get(const FCompiledMaterialPass& InPass,
                                                                      const FRenderItem& InItem)
{
	const auto Key = std::pair{&InPass, InItem.SharedParameters.get()};
	if (InItem.SharedParameters)
	{
		if (const auto It = Entries.find(Key); It != Entries.end())
		{
			return It->second.Values;
		}
	}
	auto Values = std::make_shared<FRenderBatchValues>();
	bool bCovered = bool(InItem.SharedParameters && InItem.SharedParameters->Values);
	for (const auto& Binding : InPass.Bindings)
	{
		if (!Binding.ResourceParameter && !Binding.InstanceStride)
		{
			for (const auto& Member : Binding.Members)
			{
				Values->push_back(InItem.GetMaterialValue(Member.ParameterIndex));
				bCovered = bCovered && InItem.SharedParameters->Values->Get(Member.ParameterIndex).has_value();
			}
		}
	}
	if (bCovered && Entries.size() < 128)
	{
		Entries.emplace(Key, FEntry{InItem.SharedParameters, Values});
	}
	return Values;
}

namespace
{
void RefreshSharedValues(std::shared_ptr<FRenderBatchCandidate>& InCandidate, const FRenderItem& InItem,
                         FSharedBatchValueCache& InShared)
{
	const auto Values = InShared.Get(*InCandidate->Pass, InItem);
	// Candidate metadata is private to the cache outside planning. A live group keeps its old proof.
	if (InCandidate.use_count() != 1)
	{
		InCandidate = std::make_shared<FRenderBatchCandidate>(*InCandidate);
	}
	InCandidate->Signature.SharedValues = Values;
}
} // namespace

void FRenderBatchSystem::FImpl::CanonicalizeStructure(FRenderBatchSignature& InSignature)
{
	const auto Hash = InSignature.Hash();
	const auto [Begin, End] = Structures.equal_range(Hash);
	for (auto It = Begin; It != End; ++It)
	{
		if (auto Existing = It->second.lock(); Existing && *Existing == *InSignature.Structure)
		{
			InSignature.Structure = std::move(Existing);
			return;
		}
	}
	if (Limits.MaxItems)
	{
		while (Structures.size() >= Limits.MaxItems)
		{
			Structures.erase(Structures.begin());
		}
		Structures.emplace(Hash, InSignature.Structure);
	}
}

FBatchItemKey BatchItemKey(const FRenderSceneSnapshot& InSnapshot, const FRenderItem& InItem)
{
	return {InSnapshot.View.Identity,       InSnapshot.View.Usage,       InItem.Primitive.Scene,
	        InItem.Primitive.Slot,          InItem.Primitive.Generation, InItem.LocalItemId.value_or(InItem.Ordinal),
	        InSnapshot.View.DepthConvention};
}

std::shared_ptr<const FRenderBatchCandidate> FRenderBatchSystem::FImpl::Describe(const FRenderSceneSnapshot& InSnapshot,
                                                                                 const FRenderItem& InItem,
                                                                                 FGraphicsTarget InTarget,
                                                                                 FRenderBatchStats& OutStats,
                                                                                 FSharedBatchValueCache& InShared)
{
	HYP_PERF_SCOPE_C(Detail, DescribeCachedBatchItem);
	const auto Key = BatchItemKey(InSnapshot, InItem);
	const auto Existing = Items.find(Key);
	if (InItem.LocalItemId && Existing != Items.end())
	{
		auto& Entry = Existing->second;
		if (InItem.LocalPreparation && Entry.LocalPreparation.lock() == InItem.LocalPreparation &&
		    Entry.Candidate->Signature.Structure->Target == InTarget)
		{
			RefreshSharedValues(Entry.Candidate, InItem, InShared);
			++OutStats.CompatibilityReuses;
			++OutStats.LocalCompatibilityReuses;
			RecentItems.splice(RecentItems.end(), RecentItems, Entry.Recent);
			return Entry.Candidate;
		}
		const bool bSameValues =
		    Entry.Values.lock() == InItem.ResolvedParameters && SameBatchOwner(Entry.Shared, InItem.SharedParameters);
		const bool bSameResources =
		    bSameValues || (InItem.ResolvedParameters->ResourceIdentity &&
		                    Entry.Resources.lock() == InItem.ResolvedParameters->ResourceIdentity);
		if (Entry.Lifetime.lock() == InItem.Lifetime && bSameResources && Entry.Section == InItem.State.Section &&
		    Entry.bMirrored == (Determinant(InItem.State.World) < 0) && Entry.Dynamic == InItem.DynamicState &&
		    Entry.Candidate->Program == InItem.State.Surface->GetCompiled() &&
		    Entry.Candidate->Signature.Structure->GeometryIdentity == InItem.State.Resource->GetIdentity() &&
		    Entry.Candidate->Signature.Structure->Target == InTarget)
		{
			if (!bSameValues)
			{
				RefreshSharedValues(Entry.Candidate, InItem, InShared);
				Entry.Values = InItem.ResolvedParameters;
				Entry.Shared = InItem.SharedParameters;
			}
			++OutStats.CompatibilityReuses;
			Entry.LocalPreparation = InItem.LocalPreparation;
			RecentItems.splice(RecentItems.end(), RecentItems, Entry.Recent);
			return Entry.Candidate;
		}
	}
	++OutStats.CompatibilityBuilds;
	auto Result = std::make_shared<FRenderBatchCandidate>(DescribeBatchCandidate(InItem, InSnapshot.View, InTarget));
	CanonicalizeStructure(Result->Signature);
	Result->Signature.SharedValues = InShared.Get(*Result->Pass, InItem);
	if (InItem.LocalItemId && InItem.Lifetime && Limits.MaxItems != 0)
	{
		if (Existing != Items.end())
		{
			RecentItems.erase(Existing->second.Recent);
			Items.erase(Existing);
		}
		while (Items.size() >= Limits.MaxItems)
		{
			Items.erase(RecentItems.front());
			RecentItems.pop_front();
			++OutStats.Evictions;
		}
		RecentItems.push_back(Key);
		Items.emplace(Key, FItemEntry{InItem.ResolvedParameters, InItem.ResolvedParameters->ResourceIdentity,
		                              InItem.Lifetime, Result, InItem.State.Section,
		                              Determinant(InItem.State.World) < 0, InItem.DynamicState,
		                              std::prev(RecentItems.end()), InItem.SharedParameters, InItem.LocalPreparation});
	}
	return Result;
}

std::shared_ptr<const FInstanceBatchData> FRenderBatchSystem::FImpl::Data(const FRenderSceneSnapshot& InSnapshot,
                                                                          const FRenderBatch& InBatch,
                                                                          FRenderBatchStats& OutStats)
{
	HYP_PERF_SCOPE_C(Detail, PrepareBatchInstanceData);
	const auto Key = BatchItemKey(InSnapshot, InSnapshot.Items[InBatch.Items.front()]);
	const auto Existing = Chunks.find(Key);
	bool bStable = true;
	for (const auto Index : InBatch.Items)
	{
		const auto& Item = InSnapshot.Items[Index];
		bStable &= Item.LocalItemId.has_value() && bool(Item.Lifetime) && bool(CurrentInputs[Index]);
	}
	if (bStable && Existing != Chunks.end())
	{
		auto& Entry = Existing->second;
		if (Entry.Members.size() == InBatch.Items.size() && Entry.Data->IsLive() &&
		    std::equal(Entry.Members.begin(), Entry.Members.end(), InBatch.Items.begin(),
		               [&](const auto& InMember, auto InIndex)
		               {
			               return InMember.Get() == CurrentInputs[InIndex].get();
		               }))
		{
			++OutStats.ReusedChunks;
			Entry.Access = Access;
			RecentChunks.splice(RecentChunks.end(), RecentChunks, Entry.Recent);
			return Entry.Data;
		}
	}
	auto Result = Packing.Pack(InSnapshot, InBatch.Items, OutStats);
	++OutStats.RebuiltChunks;
	const std::size_t Bytes =
	    sizeof(FChunkEntry) + Result->ByteSize() + InBatch.Items.size() * sizeof(FPreparedReference);
	if (Existing != Chunks.end())
	{
		EraseChunk(Existing);
	}
	const auto ChunkBudget = Limits.MaxBytes / 2;
	if (bStable && Limits.MaxChunks && Bytes <= ChunkBudget)
	{
		while (Chunks.size() >= Limits.MaxChunks)
		{
			EraseChunk(Chunks.find(RecentChunks.front()));
			++OutStats.Evictions;
		}
		ReserveChunkBytes(Bytes, OutStats);
		FChunkEntry Entry;
		Entry.Members.reserve(InBatch.Items.size());
		for (const auto Index : InBatch.Items)
		{
			Entry.Members.push_back(CurrentInputs[Index]);
		}
		Entry.Data = Result;
		Entry.Bytes = Bytes;
		Entry.Access = Access;
		RecentChunks.push_back(Key);
		Entry.Recent = std::prev(RecentChunks.end());
		Chunks.emplace(Key, std::move(Entry));
		ChunkBytes += Bytes;
	}
	return Result;
}

void FRenderBatchSystem::FImpl::EraseChunk(std::map<FBatchItemKey, FChunkEntry>::iterator InEntry)
{
	ChunkBytes -= InEntry->second.Bytes;
	RecentChunks.erase(InEntry->second.Recent);
	Chunks.erase(InEntry);
}

void FRenderBatchSystem::FImpl::ReserveChunkBytes(std::size_t InBytes, FRenderBatchStats& OutStats)
{
	while (!Chunks.empty() && ChunkBytes + InBytes > Limits.MaxBytes / 2)
	{
		EraseChunk(Chunks.find(RecentChunks.front()));
		++OutStats.Evictions;
	}
}

void FRenderBatchSystem::FImpl::RetireExpired()
{
	HYP_PERF_SCOPE_C(Detail, RetireBatchFamily);
	std::erase_if(Structures,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.expired();
	              });
	std::erase_if(Contracts,
	              [](const auto& InEntry)
	              {
		              return InEntry.second->Program.expired();
	              });
	CollectPrepared();
	Packing.Collect();
	for (auto It = Plans.begin(); It != Plans.end();)
	{
		if (std::any_of(It->second.Inputs.begin(), It->second.Inputs.end(),
		                [](const auto& InItem)
		                {
			                const auto* Input = InItem.Input.Get();
			                return !Input || Input->Lifetime.expired();
		                }))
		{
			PlanItems -= It->second.Inputs.size();
			It = Plans.erase(It);
		}
		else
		{
			++It;
		}
	}
	for (auto It = Items.begin(); It != Items.end();)
	{
		if (It->second.Lifetime.expired())
		{
			const auto Input = Prepared.find(It->first);
			if (Input != Prepared.end() && Input->second.Input->Lifetime.expired())
			{
				ErasePrepared(Input);
			}
			RecentItems.erase(It->second.Recent);
			It = Items.erase(It);
		}
		else
		{
			++It;
		}
	}
	for (auto It = Chunks.begin(); It != Chunks.end();)
	{
		if (!It->second.Data->IsLive())
		{
			const auto Previous = It++;
			EraseChunk(Previous);
		}
		else
		{
			++It;
		}
	}
}

void FRenderBatchSystem::FImpl::Retire(const FRenderSceneSnapshot& InSnapshot)
{
	const std::array Family{InSnapshot.Frame ? InSnapshot.Frame->Session : 0,
	                        InSnapshot.Frame ? InSnapshot.Frame->Frame : 0, InSnapshot.Family};
	if (!InSnapshot.Frame || Family != RetiredFamily)
	{
		const bool bStableSources = InSnapshot.Frame && InSnapshot.LocalContentIdentity &&
		                            InSnapshot.CollectionKey[0] && InSnapshot.CollectionKey[1] &&
		                            InSnapshot.CollectionKey[2] && InSnapshot.CollectionKey == RetiredCollection;
		// Static published sources keep their source leases and local proofs. Bound redundant CPU sweeps;
		// source/publication changes and unproven inputs still sweep immediately. Lookups, payload limits,
		// per-view chunk retirement below and native fence collection remain active on every use.
		if (!bStableSources || ++RetainedFamilies >= 64)
		{
			RetireExpired();
			RetainedFamilies = 0;
		}
		RetiredFamily = Family;
		RetiredCollection = InSnapshot.CollectionKey;
	}
	// Only the current view's small chunk range needs visibility retirement after each build.
	const FBatchItemKey First{InSnapshot.View.Identity, InSnapshot.View.Usage, 0, 0, 0, 0, EDepthConvention::Standard};
	for (auto It = Chunks.lower_bound(First); It != Chunks.end() &&
	                                          std::get<0>(It->first) == InSnapshot.View.Identity &&
	                                          std::get<1>(It->first) == InSnapshot.View.Usage;)
	{
		if (It->second.Access != Access || !It->second.Data->IsLive())
		{
			const auto Previous = It++;
			EraseChunk(Previous);
		}
		else
		{
			++It;
		}
	}
}
} // namespace Hyperion
