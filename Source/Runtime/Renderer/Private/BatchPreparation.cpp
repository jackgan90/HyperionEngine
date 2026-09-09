#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "RenderBatchInternal.h"
#include "SceneItemPreparation.h"

namespace Hyperion
{
bool FRenderBatchSystem::FImpl::FInstanceValueReference::Matches(
    const std::shared_ptr<const FMaterialValue>& InValue) const
{
	// The current strong value proves this identity is live. Never dereference the cached address: value
	// owners can disappear on other threads, unlike Render-owned preparations. Expired nonempty is not null.
	if (Address == InValue.get() && !Owner.owner_before(InValue) && !InValue.owner_before(Owner))
	{
		return true;
	}
	const auto Value = Owner.lock();
	return Value && SameMaterialValue(Value, InValue);
}

bool FRenderBatchSystem::FImpl::FPreparedItem::MatchesSource(const FRenderItem& InItem) const
{
	return InItem.PreparationError.empty() && InItem.Primitive == Primitive && InItem.LocalItemId == LocalId &&
	       InItem.Lifetime && InItem.State.Surface && InItem.ResolvedParameters && Lifetime.lock() == InItem.Lifetime &&
	       Surface.lock() == InItem.State.Surface;
}

bool FRenderBatchSystem::FImpl::FPreparedItem::MatchesState(const FRenderItem& InItem) const
{
	return MatchesSource(InItem) && InItem.State.Resource && Resource.lock() == InItem.State.Resource &&
	       Section == InItem.State.Section && bMirrored == (Determinant(InItem.State.World) < 0) &&
	       Dynamic == InItem.DynamicState;
}

bool FRenderBatchSystem::FImpl::FPreparedItem::MatchesValues(const FRenderItem& InItem) const
{
	if (Values.lock() == InItem.ResolvedParameters)
	{
		if (SameBatchOwner(Shared, InItem.SharedParameters))
		{
			return true;
		}
	}
	else if (!InItem.ResolvedParameters || !InItem.ResolvedParameters->ResourceIdentity ||
	         Resources.lock() != InItem.ResolvedParameters->ResourceIdentity ||
	         !LocalValues.Matches(InItem.ResolvedParameters->Values))
	{
		return false;
	}
	return MatchesInstances(InItem);
}

bool FRenderBatchSystem::FImpl::FPreparedItem::MatchesInstances(const FRenderItem& InItem) const
{
	for (std::size_t Index = 0; Index < Contract->Parameters.size(); ++Index)
	{
		if (!InstanceValues[Index].Matches(InItem.GetMaterialValue(Contract->Parameters[Index])))
		{
			return false;
		}
	}
	return true;
}

void FRenderBatchSystem::FImpl::FPreparedItem::RefreshValues(const FRenderItem& InItem)
{
	if (Values.lock() != InItem.ResolvedParameters)
	{
		LocalValues = InItem.ResolvedParameters->Values.GetLocalIdentity();
		Values = InItem.ResolvedParameters;
		Resources = InItem.ResolvedParameters->ResourceIdentity;
	}
	Shared = InItem.SharedParameters;
	Resource = InItem.State.Resource;
	Section = InItem.State.Section;
	bMirrored = Determinant(InItem.State.World) < 0;
	Dynamic = InItem.DynamicState;
}

std::shared_ptr<const FRenderBatchSystem::FImpl::FInstanceContract> FRenderBatchSystem::FImpl::Contract(
    const FRenderSceneSnapshot& InSnapshot, const FRenderItem& InItem, FRenderBatchStats& OutStats)
{
	const auto Program = InItem.Preparation && InItem.Preparation->Program ? InItem.Preparation->Program
	                                                                       : InItem.State.Surface->GetCompiled();
	const auto Key = std::pair{Program.get(), InSnapshot.View.Usage};
	if (const auto It = Contracts.find(Key); It != Contracts.end() && It->second->Program.lock() == Program)
	{
		return It->second;
	}
	auto Result = std::make_shared<FInstanceContract>();
	Result->Program = Program;
	if (const auto* Pass = Program->FindInstancePass(InSnapshot.View.Usage))
	{
		for (const auto& Binding : Pass->Bindings)
		{
			if (Binding.InstanceStride)
			{
				for (const auto& Member : Binding.Members)
				{
					Result->Parameters.push_back(Member.ParameterIndex);
				}
			}
		}
	}
	++OutStats.InstanceContractBuilds;
	while (Contracts.size() >= 256)
	{
		Contracts.erase(Contracts.begin());
	}
	Contracts.insert_or_assign(Key, Result);
	return Result;
}

std::shared_ptr<const FRenderBatchSystem::FImpl::FPreparedItem> FRenderBatchSystem::FImpl::PrepareInput(
    const FRenderSceneSnapshot& InSnapshot, const FRenderItem& InItem, FRenderBatchStats& OutStats)
{
	const auto Key = BatchItemKey(InSnapshot, InItem);
	const auto Existing = Prepared.find(Key);
	if (InItem.LocalItemId && Existing != Prepared.end())
	{
		const auto& Entry = Existing->second;
		if (Entry.Input->MatchesSource(InItem) &&
		    (Entry.Input->MatchesValues(InItem) || Entry.Input->MatchesInstances(InItem)))
		{
			Entry.Input->RefreshValues(InItem);
			++OutStats.PreparedInputReuses;
			RecentPrepared.splice(RecentPrepared.end(), RecentPrepared, Entry.Recent);
			return Entry.Input;
		}
	}
	auto Result = std::make_shared<FPreparedItem>();
	Result->Primitive = InItem.Primitive;
	Result->LocalId = InItem.LocalItemId.value_or(0);
	Result->Lifetime = InItem.Lifetime;
	Result->Resource = InItem.State.Resource;
	Result->Surface = InItem.State.Surface;
	Result->Values = InItem.ResolvedParameters;
	Result->LocalValues = InItem.ResolvedParameters->Values.GetLocalIdentity();
	Result->Resources = InItem.ResolvedParameters->ResourceIdentity;
	Result->Contract = Contract(InSnapshot, InItem, OutStats);
	Result->Section = InItem.State.Section;
	Result->bMirrored = Determinant(InItem.State.World) < 0;
	Result->Dynamic = InItem.DynamicState;
	Result->Shared = InItem.SharedParameters;
	Result->InstanceValues.reserve(Result->Contract->Parameters.size());
	// These count-bounded proofs own no numeric trees. Track their metadata separately from packed data,
	// so a small payload budget cannot destroy otherwise reusable ordinary-draw plans.
	Result->MetadataBytes = sizeof(FPreparedItem) +
	                        InItem.ResolvedParameters->Values.GetSize() *
	                            (sizeof(std::weak_ptr<const void>) + sizeof(std::uint64_t) + sizeof(std::size_t)) +
	                        Result->InstanceValues.capacity() * sizeof(FInstanceValueReference);
	for (const auto Index : Result->Contract->Parameters)
	{
		Result->InstanceValues.emplace_back(InItem.GetMaterialValue(Index));
	}
	++OutStats.PreparedInputBuilds;
	if (Existing != Prepared.end())
	{
		ErasePrepared(Existing);
	}
	if (InItem.LocalItemId && InItem.Lifetime && Limits.MaxItems)
	{
		while (Prepared.size() >= Limits.MaxItems)
		{
			ErasePrepared(Prepared.find(RecentPrepared.front()));
			++OutStats.Evictions;
		}
		RecentPrepared.push_back(Key);
		try
		{
			Prepared.emplace(Key, FPreparedEntry{Result, std::prev(RecentPrepared.end())});
		}
		catch (...)
		{
			RecentPrepared.pop_back();
			throw;
		}
		PreparedMetadataBytes += Result->MetadataBytes;
	}
	return Result;
}

void FRenderBatchSystem::FImpl::PrepareInputs(const FRenderSceneSnapshot& InSnapshot, FRenderBatchStats& OutStats)
{
	HYP_PERF_SCOPE_C(Detail, PrepareBatchInputs);
	CurrentInputs.clear();
	CurrentInputs.resize(InSnapshot.Items.Size());
	for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
	{
		const auto& Item = InSnapshot.Items[Index];
		if (Item.PreparationError.empty() && Item.State.Surface && Item.State.Resource && Item.ResolvedParameters)
		{
			try
			{
				CurrentInputs[Index] = PrepareInput(InSnapshot, Item, OutStats);
			}
			catch (const std::exception&)
			{
				// The regular candidate/packing path retains per-item fallback diagnostics.
			}
		}
	}
}

void FRenderBatchSystem::FImpl::ErasePrepared(std::map<FBatchItemKey, FPreparedEntry>::iterator InEntry)
{
	PreparedMetadataBytes -= InEntry->second.Input->MetadataBytes;
	RecentPrepared.erase(InEntry->second.Recent);
	Prepared.erase(InEntry);
}

void FRenderBatchSystem::FImpl::CollectPrepared()
{
	// The existing candidate retirement scan removes matching dead inputs immediately. Sweep a bounded
	// portion here for orphaned preparations whose candidate was evicted earlier. These own only weak value
	// proofs; insertion-time item eviction always enforces the metadata count limit independently of this sweep.
	auto It = PreparedCursor ? Prepared.lower_bound(*PreparedCursor) : Prepared.begin();
	if (It == Prepared.end())
	{
		It = Prepared.begin();
	}
	for (std::size_t Count = 0; Count < 64 && It != Prepared.end(); ++Count)
	{
		if (It->second.Input->Lifetime.expired())
		{
			ErasePrepared(It++);
		}
		else
		{
			++It;
		}
	}
	PreparedCursor = It == Prepared.end() ? std::nullopt : std::optional<FBatchItemKey>(It->first);
}
} // namespace Hyperion
