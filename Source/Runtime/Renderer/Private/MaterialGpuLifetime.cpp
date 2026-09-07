#include "MaterialGpuCacheInternal.h"

namespace Hyperion
{
bool FMaterialCacheOwnership::IsUsed()
{
	std::erase_if(Uses,
	              [](const auto& InUse)
	              {
		              return std::any_of(InUse.begin(), InUse.end(),
		                                 [](const auto& InOwner)
		                                 {
			                                 return InOwner.expired();
		                                 });
	              });
	return !Uses.empty();
}

void FMaterialCacheOwnership::Add(const FMaterialResourceOwners& InOwners)
{
	if (InOwners.empty() || std::any_of(InOwners.begin(), InOwners.end(),
	                                    [](const auto& InOwner)
	                                    {
		                                    return !InOwner;
	                                    }))
	{
		throw std::invalid_argument("Material cache entries require complete CPU resource ownership");
	}
	if (Uses.find(InOwners) == Uses.end())
	{
		Uses.emplace(InOwners.begin(), InOwners.end());
	}
}

namespace
{
template<typename TEntry> bool CanRetire(TEntry& InEntry, bool& bOutPending)
{
	if (InEntry.Ownership.IsUsed())
	{
		return false;
	}
	if (InEntry.Resource && InEntry.Resource.Payload.use_count() != 1)
	{
		bOutPending = true;
		return false;
	}
	return true;
}

template<typename TMap> void CollectEntries(TMap& InEntries, bool& bOutPending)
{
	for (auto Iterator = InEntries.begin(); Iterator != InEntries.end();)
	{
		if (CanRetire(Iterator->second, bOutPending))
		{
			Iterator = InEntries.erase(Iterator);
		}
		else
		{
			++Iterator;
		}
	}
}

template<typename TMap> void ClearEntryOwners(TMap& InEntries)
{
	for (auto& [Key, Entry] : InEntries)
	{
		Entry.Ownership.Uses.clear();
	}
}
} // namespace

bool FMaterialGpuCache::Collect()
{
	Impl->CheckOwner();
	bool bPending = false;
	// Consumers retire first. Their native payloads and cached descriptions retain layouts and source descriptors.
	CollectEntries(Impl->Sets, bPending);
	CollectEntries(Impl->Pipelines, bPending);
	CollectEntries(Impl->Layouts, bPending);
	for (auto Iterator = Impl->Textures.begin(); Iterator != Impl->Textures.end();)
	{
		auto& Entry = Iterator->second;
		if (Entry.Resource && !Impl->Device.TexturesReady(std::span(&Entry.Resource, 1)))
		{
			bPending = true;
			++Iterator;
		}
		else if (CanRetire(Entry, bPending))
		{
			Iterator = Impl->Textures.erase(Iterator);
		}
		else
		{
			++Iterator;
		}
	}
	CollectEntries(Impl->Buffers, bPending);
	std::erase_if(Impl->Samplers,
	              [&](FImpl::FSamplerEntry& InEntry)
	              {
		              return CanRetire(InEntry, bPending);
	              });
	return bPending;
}

void FMaterialGpuCache::ClearOwners()
{
	Impl->CheckOwner();
	ClearEntryOwners(Impl->Sets);
	ClearEntryOwners(Impl->Pipelines);
	ClearEntryOwners(Impl->Layouts);
	ClearEntryOwners(Impl->Textures);
	ClearEntryOwners(Impl->Buffers);
	for (auto& Entry : Impl->Samplers)
	{
		Entry.Ownership.Uses.clear();
	}
	Collect();
}

bool FMaterialGpuCache::IsEmpty() const
{
	Impl->CheckOwner();
	return Impl->Sets.empty() && Impl->Pipelines.empty() && Impl->Layouts.empty() && Impl->Textures.empty() &&
	       Impl->Buffers.empty() && Impl->Samplers.empty();
}

FMaterialGpuStats FMaterialGpuCache::Statistics() const
{
	Impl->CheckOwner();
	auto Result = Impl->Stats;
	Result.LiveObjects = Impl->Sets.size() + Impl->Pipelines.size() + Impl->Layouts.size() + Impl->Textures.size() +
	                     Impl->Buffers.size() + Impl->Samplers.size();
	return Result;
}
} // namespace Hyperion
