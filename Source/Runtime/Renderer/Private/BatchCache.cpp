#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/MaterialInputValues.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "RenderBatchInternal.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
std::shared_ptr<const FRenderBatchCandidate> RefreshSharedValues(
    const std::shared_ptr<const FRenderBatchCandidate>& InCandidate, const FResolvedMaterialParameters& InValues)
{
	std::vector<std::shared_ptr<const FMaterialValue>> Values;
	for (const auto& Binding : InCandidate->Pass->Bindings)
	{
		if (!Binding.ResourceParameter && !Binding.InstanceStride)
		{
			for (const auto& Member : Binding.Members)
			{
				Values.push_back(InValues.Values[Member.ParameterIndex]);
			}
		}
	}
	if (std::equal(Values.begin(), Values.end(), InCandidate->Signature.SharedValues.begin(),
	               InCandidate->Signature.SharedValues.end(),
	               [](const auto& InA, const auto& InB)
	               {
		               return SameMaterialValue(InA, InB);
	               }))
	{
		return InCandidate;
	}
	auto Result = std::make_shared<FRenderBatchCandidate>(*InCandidate);
	Result->Signature.SharedValues = std::move(Values);
	return Result;
}
} // namespace

FBatchItemKey BatchItemKey(const FRenderSceneSnapshot& InSnapshot, const FRenderItem& InItem)
{
	return {InSnapshot.View.Identity, InSnapshot.View.Usage,       InItem.Primitive.Scene,
	        InItem.Primitive.Slot,    InItem.Primitive.Generation, InItem.LocalItemId.value_or(InItem.Ordinal)};
}

std::shared_ptr<const FRenderBatchCandidate> FRenderBatchSystem::FImpl::Describe(const FRenderSceneSnapshot& InSnapshot,
                                                                                 const FRenderItem& InItem,
                                                                                 FGraphicsTarget InTarget,
                                                                                 FRenderBatchStats& OutStats)
{
	const auto Key = BatchItemKey(InSnapshot, InItem);
	const auto Existing = Items.find(Key);
	const bool bMirrored = Determinant(InItem.State.World) < 0;
	if (InItem.LocalItemId && Existing != Items.end())
	{
		auto& Entry = Existing->second;
		const bool bSameValues = Entry.Values.lock() == InItem.ResolvedParameters;
		const bool bSameResources =
		    bSameValues || (InItem.ResolvedParameters->ResourceIdentity &&
		                    Entry.Resources.lock() == InItem.ResolvedParameters->ResourceIdentity);
		if (Entry.Lifetime.lock() == InItem.Lifetime && bSameResources && Entry.Section == InItem.State.Section &&
		    Entry.bMirrored == bMirrored && Entry.Dynamic == InItem.DynamicState &&
		    Entry.Candidate->Program == InItem.State.Surface->GetCompiled() &&
		    Entry.Candidate->Signature.GeometryIdentity == InItem.State.Resource->GetIdentity() &&
		    Entry.Candidate->Signature.Target == InTarget)
		{
			if (!bSameValues)
			{
				Entry.Candidate = RefreshSharedValues(Entry.Candidate, *InItem.ResolvedParameters);
				Entry.Values = InItem.ResolvedParameters;
			}
			++OutStats.CompatibilityReuses;
			RecentItems.splice(RecentItems.end(), RecentItems, Entry.Recent);
			return Entry.Candidate;
		}
	}
	++OutStats.CompatibilityBuilds;
	auto Result =
	    std::make_shared<const FRenderBatchCandidate>(DescribeBatchCandidate(InItem, InSnapshot.View, InTarget));
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
		                              InItem.Lifetime, Result, InItem.State.Section, bMirrored, InItem.DynamicState,
		                              std::prev(RecentItems.end())});
	}
	return Result;
}

namespace
{
std::vector<std::shared_ptr<const FMaterialValue>> InstanceValues(const FRenderSceneSnapshot& InSnapshot,
                                                                  const FRenderBatch& InBatch)
{
	std::vector<std::shared_ptr<const FMaterialValue>> Result;
	for (const auto Index : InBatch.Items)
	{
		const auto& Item = InSnapshot.Items[Index];
		const auto Program = Item.State.Surface->GetCompiled();
		for (const auto& Binding : Program->GetPass(InSnapshot.View.Usage, "Instance").Bindings)
		{
			if (Binding.InstanceStride)
			{
				for (const auto& Member : Binding.Members)
				{
					Result.push_back(Item.ResolvedParameters->Values[Member.ParameterIndex]);
				}
			}
		}
	}
	return Result;
}

bool SameValues(const std::vector<std::shared_ptr<const FMaterialValue>>& InA,
                const std::vector<std::shared_ptr<const FMaterialValue>>& InB)
{
	return InA.size() == InB.size() && std::equal(InA.begin(), InA.end(), InB.begin(),
	                                              [](const auto& InLeft, const auto& InRight)
	                                              {
		                                              return SameMaterialValue(InLeft, InRight);
	                                              });
}
} // namespace

std::shared_ptr<const FInstanceBatchData> FRenderBatchSystem::FImpl::Data(const FRenderSceneSnapshot& InSnapshot,
                                                                          const FRenderBatch& InBatch,
                                                                          FRenderBatchStats& OutStats)
{
	const auto& First = InSnapshot.Items[InBatch.Items.front()];
	const auto Program = First.State.Surface->GetCompiled();
	const auto& Pass = Program->GetPass(InSnapshot.View.Usage, "Instance");
	const auto Layout = DescribeMaterialLayout(Pass);
	std::vector<FBatchItemKey> Members;
	bool bStable = true;
	for (const auto Index : InBatch.Items)
	{
		const auto& Item = InSnapshot.Items[Index];
		Members.push_back(BatchItemKey(InSnapshot, Item));
		bStable &= Item.LocalItemId.has_value() && bool(Item.Lifetime);
	}
	auto Values = InstanceValues(InSnapshot, InBatch);
	const auto Key = Members.front();
	const auto Existing = Chunks.find(Key);
	if (bStable && Existing != Chunks.end())
	{
		auto& Entry = Existing->second;
		if (Entry.Members == Members && Entry.Vertex == Pass.Vertex.CacheKey && Entry.Pixel == Pass.Pixel.CacheKey &&
		    Entry.Layout == Layout && Entry.Data->IsLive() && SameValues(Entry.Values, Values))
		{
			++OutStats.ReusedChunks;
			Entry.Access = Access;
			RecentChunks.splice(RecentChunks.end(), RecentChunks, Entry.Recent);
			return Entry.Data;
		}
	}
	auto Result = PackInstanceBatch(InSnapshot, InBatch.Items);
	++OutStats.RebuiltChunks;
	OutStats.PackedBytes += Result->ByteSize();
	std::size_t Bytes = sizeof(FChunkEntry) + Result->ByteSize() + Members.capacity() * sizeof(FBatchItemKey) +
	                    Values.capacity() * sizeof(std::shared_ptr<const FMaterialValue>);
	for (const auto& Value : Values)
	{
		Bytes += Value ? MaterialValueStorageBytes(*Value) : 0;
	}
	if (Existing != Chunks.end())
	{
		EraseChunk(Existing);
	}
	if (bStable && Limits.MaxChunks && Bytes <= Limits.MaxBytes)
	{
		while (!RecentChunks.empty() && (Chunks.size() >= Limits.MaxChunks || ChunkBytes + Bytes > Limits.MaxBytes))
		{
			EraseChunk(Chunks.find(RecentChunks.front()));
			++OutStats.Evictions;
		}
		RecentChunks.push_back(Key);
		Chunks.emplace(Key, FChunkEntry{std::move(Members), Pass.Vertex.CacheKey, Pass.Pixel.CacheKey, Layout,
		                                std::move(Values), Result, Bytes, Access, std::prev(RecentChunks.end())});
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

void FRenderBatchSystem::FImpl::RetireExpired()
{
	HYP_PERF_SCOPE_C(Detail, RetireBatchFamily);
	for (auto It = Plans.begin(); It != Plans.end();)
	{
		if (std::any_of(It->second.Inputs.begin(), It->second.Inputs.end(),
		                [](const auto& InItem)
		                {
			                return InItem.Lifetime.expired();
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
		RetireExpired();
		RetiredFamily = Family;
	}
	// Only the current view's small chunk range needs visibility retirement after each build.
	const FBatchItemKey First{InSnapshot.View.Identity, InSnapshot.View.Usage, 0, 0, 0, 0};
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
