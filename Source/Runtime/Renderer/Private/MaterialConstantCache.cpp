#include "Hyperion/Renderer/MaterialConstantCache.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/MaterialPacking.h"
#include "Hyperion/Renderer/RenderBatch.h"
#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <thread>

namespace Hyperion
{
namespace
{
struct FConstantKey
{
	EShaderFormat Format{};
	std::uint32_t Size{};
	std::vector<FShaderMember> Layout;
	std::vector<std::string> Mapping;
	std::vector<std::shared_ptr<const FMaterialValue>> Values;
	std::vector<std::pair<EMaterialScope, FMaterialScopeKey>> Scopes;
};

std::uint64_t HashBinding(const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
                          const FMaterialParameterSchema& InSchema, const FResolvedMaterialParameters& InParameters)
{
	// Full structured equality follows every hash hit. The hash only selects a small candidate bucket.
	std::uint64_t Hash = 14695981039346656037ULL;
	const auto Add = [&](std::uint64_t InValue)
	{
		Hash = (Hash ^ InValue) * 1099511628211ULL;
	};
	Add(InBinding.Resource.ByteSize);
	Add(static_cast<unsigned>(InFormat));
	std::uint32_t Dependencies{};
	for (const auto& Member : InBinding.Members)
	{
		Add(Member.Layout.Offset);
		Add(Member.Layout.Size);
		Add(Member.Layout.Rows);
		Add(Member.Layout.Columns);
		Dependencies |= InParameters.Dependencies.Get(Member.ParameterIndex);
	}
	for (const auto& Member : InBinding.Members)
	{
		const auto& Parameter = InSchema.GetParameters().at(Member.ParameterIndex);
		Add(Parameter.Name.size() + 1 + Parameter.Semantic.size());
		for (const unsigned char Character : Parameter.Name)
		{
			Add(Character);
		}
		Add(0);
		for (const unsigned char Character : Parameter.Semantic)
		{
			Add(Character);
		}
	}
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((Dependencies & (1U << Scope)) == 0)
		{
			continue;
		}
		const auto& Input = InParameters.Scopes[Scope];
		const auto& Key = Input.Key;
		if (!Input.Lifetime || Key.Identity == 0 || Key.Revision == 0)
		{
			throw std::invalid_argument("Material constant dependency requires an owned scope identity and revision");
		}
		Add(static_cast<unsigned>(Scope));
		Add(Key.Identity);
		Add(Key.Revision);
		for (const auto Qualifier : Key.GetQualifiers())
		{
			Add(Qualifier);
		}
	}
	return Hash;
}

bool MatchesBinding(const FConstantKey& InKey, const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
                    const FMaterialParameterSchema& InSchema, const FResolvedMaterialParameters& InParameters,
                    bool bInCompareValues = true)
{
	if (InKey.Format != InFormat || InKey.Size != InBinding.Resource.ByteSize ||
	    InKey.Layout.size() != InBinding.Members.size())
	{
		return false;
	}
	std::uint32_t Dependencies{};
	for (std::size_t Index = 0; Index < InBinding.Members.size(); ++Index)
	{
		const auto& Member = InBinding.Members[Index];
		const auto& Parameter = InSchema.GetParameters().at(Member.ParameterIndex);
		const std::string_view Mapping = InKey.Mapping[Index];
		if (InKey.Layout[Index] != Member.Layout ||
		    (bInCompareValues &&
		     !SameMaterialValue(InKey.Values[Index], InParameters.Values.Get(Member.ParameterIndex))) ||
		    Mapping.size() != Parameter.Name.size() + 1 + Parameter.Semantic.size() ||
		    !Mapping.starts_with(Parameter.Name) || Mapping[Parameter.Name.size()] != '\0' ||
		    Mapping.substr(Parameter.Name.size() + 1) != Parameter.Semantic)
		{
			return false;
		}
		Dependencies |= InParameters.Dependencies.Get(Member.ParameterIndex);
	}
	std::size_t Index{};
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((Dependencies & (1U << Scope)) != 0)
		{
			if (Index >= InKey.Scopes.size() || InKey.Scopes[Index].first != static_cast<EMaterialScope>(Scope) ||
			    InKey.Scopes[Index].second != InParameters.Scopes[Scope].Key)
			{
				return false;
			}
			++Index;
		}
	}
	return Index == InKey.Scopes.size();
}

struct FCacheEntry
{
	FConstantKey Key;
	FBufferSlice Slice;
	std::uint64_t Hash{};
	std::list<FCacheEntry>::iterator Location;
	std::list<FCacheEntry*>::iterator Recency;
	std::vector<std::weak_ptr<const void>> Owners;

	bool IsExpired() const
	{
		return std::any_of(Owners.begin(), Owners.end(),
		                   [](const auto& InOwner)
		                   {
			                   return InOwner.expired();
		                   });
	}
};

FCacheEntry MakeEntry(const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
                      const FMaterialParameterSchema& InSchema, const FResolvedMaterialParameters& InParameters)
{
	FCacheEntry Result;
	auto& Key = Result.Key;
	Key.Format = InFormat;
	Key.Size = InBinding.Resource.ByteSize;
	std::uint32_t Dependencies{};
	for (const auto& Member : InBinding.Members)
	{
		Key.Layout.push_back(Member.Layout);
		const auto& Declaration = InSchema.GetParameters().at(Member.ParameterIndex);
		Key.Mapping.push_back(Declaration.Name + std::string(1, '\0') + Declaration.Semantic);
		Key.Values.push_back(InParameters.Values.Get(Member.ParameterIndex));
		Dependencies |= InParameters.Dependencies.Get(Member.ParameterIndex);
	}
	for (std::size_t Index = 0; Index < MaterialScopeCount; ++Index)
	{
		if ((Dependencies & (1U << Index)) == 0)
		{
			continue;
		}
		const auto& Input = InParameters.Scopes[Index];
		if (Input.Key.Identity == 0 || Input.Key.Revision == 0 || !Input.Lifetime)
		{
			throw std::invalid_argument("Material constant dependency requires an owned scope identity and revision");
		}
		Key.Scopes.push_back({static_cast<EMaterialScope>(Index), Input.Key});
		Result.Owners.push_back(Input.Lifetime);
	}
	if (Result.Owners.empty())
	{
		Result.Owners.push_back(InParameters.Scopes[static_cast<std::size_t>(EMaterialScope::Material)].Lifetime);
	}
	return Result;
}
} // namespace

struct FMaterialConstantCache::FImpl
{
	struct FPage
	{
		FBuffer Buffer;
		std::uint32_t End{};
		bool bTransient{};
	};

	struct FPreparedBlock
	{
		std::weak_ptr<const FCompiledMaterialDefinition> Program;
		std::vector<std::shared_ptr<const FMaterialValue>> Values;
		FBufferSlice Slice;
		std::uint64_t Access{};
	};

	struct FInstanceEntry
	{
		std::weak_ptr<const FInstanceBatchData> Data;
		std::vector<FConstantBinding> Bindings;
		std::size_t Bytes{};
		std::list<const FInstanceBatchData*>::iterator Recent;
	};

	IRHIDevice& Device;
	std::thread::id Owner = std::this_thread::get_id();
	std::uint32_t PageSize;
	std::map<std::uint64_t, std::list<FCacheEntry>> Entries;
	std::list<FCacheEntry*> Recent;
	std::vector<FPage> Pages;
	FMaterialConstantStats Stats;
	FMaterialConstantLimits Limits;
	std::function<void(const std::shared_ptr<const void>&)> TrackScope;
	std::uint64_t Access{};
	std::map<std::pair<const FCompiledMaterialDefinition*, const FMaterialProgramBinding*>, FPreparedBlock> Prepared;
	std::map<const FInstanceBatchData*, FInstanceEntry> Instances;
	std::list<const FInstanceBatchData*> RecentInstances;
	std::size_t InstanceBytes{};

	FImpl(IRHIDevice& InDevice, std::uint32_t InPageSize, FMaterialConstantLimits InLimits)
	    : Device(InDevice), PageSize(InPageSize), Limits(InLimits)
	{
	}

	void CheckOwner() const
	{
		if (Owner != std::this_thread::get_id())
		{
			throw std::logic_error("Material constant cache requires its RHI owner");
		}
	}

	FBufferSlice Publish(std::span<const std::byte> InData, bool bInTransient)
	{
		if (InData.empty() || InData.size() > PageSize || InData.size() > Device.GetCapabilities().MaxConstantRange)
		{
			throw std::invalid_argument("Constant publication exceeds device/page capacity");
		}
		const auto Alignment = Device.GetCapabilities().ConstantAlignment;
		const auto Extent = static_cast<std::uint32_t>((InData.size() + Alignment - 1) / Alignment * Alignment);
		auto Page = std::find_if(Pages.begin(), Pages.end(),
		                         [&](const FPage& InPage)
		                         {
			                         return (InPage.End == 0 || InPage.bTransient == bInTransient) &&
			                                Extent <= PageSize - InPage.End;
		                         });
		if (Page == Pages.end())
		{
			Pages.push_back({Device.CreateBuffer({PageSize, BufferUsage(ERHIBufferUsage::Constant)}), 0, bInTransient});
			Page = std::prev(Pages.end());
			++Stats.PagesCreated;
		}
		Page->bTransient = bInTransient;
		FBufferSlice Result = Device.PublishConstantSlice(Page->Buffer, Page->End, InData);
		Page->End += Extent;
		Stats.UploadBytes += InData.size();
		return Result;
	}

	void Untrack(const FCacheEntry& InEntry)
	{
		Recent.erase(InEntry.Recency);
		Stats.CachedBytes -= InEntry.Slice.Extent;
		--Stats.CachedBlocks;
	}

	void EraseInstance(std::map<const FInstanceBatchData*, FInstanceEntry>::iterator InEntry)
	{
		InstanceBytes -= InEntry->second.Bytes;
		RecentInstances.erase(InEntry->second.Recent);
		Instances.erase(InEntry);
	}

	void Trim(std::size_t InBytes)
	{
		while (!Recent.empty() &&
		       (Stats.CachedBlocks >= Limits.MaxBlocks || Stats.CachedBytes + InBytes > Limits.MaxBytes))
		{
			const auto* Entry = Recent.front();
			const auto Bucket = Entries.find(Entry->Hash);
			const auto Location = Entry->Location;
			Untrack(*Entry);
			Bucket->second.erase(Location);
			++Stats.Evictions;
			if (Bucket->second.empty())
			{
				Entries.erase(Bucket);
			}
		}
	}

	void Store(std::uint64_t InHash, FCacheEntry InEntry)
	{
		Trim(InEntry.Slice.Extent);
		const auto Bucket = Entries.try_emplace(InHash).first;
		auto& Values = Bucket->second;
		auto Location = Values.end();
		try
		{
			Location = Values.insert(Values.end(), std::move(InEntry));
			Recent.push_back(&*Location);
		}
		catch (...)
		{
			if (Location != Values.end())
			{
				Values.erase(Location);
			}
			if (Values.empty())
			{
				Entries.erase(Bucket);
			}
			throw;
		}
		auto& Stored = *Location;
		Stored.Hash = InHash;
		Stored.Location = Location;
		Stored.Recency = std::prev(Recent.end());
		Stats.CachedBytes += Stored.Slice.Extent;
		++Stats.CachedBlocks;
	}

	FBufferSlice BindShared(const std::shared_ptr<const FCompiledMaterialDefinition>& InProgram,
	                        const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
	                        const FResolvedMaterialParameters& InParameters)
	{
		const auto Key = std::pair{InProgram.get(), &InBinding};
		const auto It = Prepared.find(Key);
		if (It != Prepared.end() && It->second.Program.lock() == InProgram &&
		    It->second.Values.size() == InBinding.Members.size())
		{
			bool bSame = true;
			for (std::size_t Index = 0; bSame && Index < InBinding.Members.size(); ++Index)
			{
				bSame = It->second.Values[Index] == InParameters.Values[InBinding.Members[Index].ParameterIndex];
			}
			if (bSame)
			{
				++Stats.Reuses;
				++Stats.PreparedReuses;
				It->second.Access = ++Access;
				return It->second.Slice;
			}
		}
		FPreparedBlock Block;
		Block.Program = InProgram;
		Block.Access = ++Access;
		Block.Slice = Bind(InBinding, InFormat, *InProgram->Interface.Schema, InParameters);
		const auto Result = Block.Slice;
		if (Limits.MaxPreparedBlocks == 0)
		{
			return Result;
		}
		for (const auto& Member : InBinding.Members)
		{
			Block.Values.push_back(InParameters.Values[Member.ParameterIndex]);
		}
		if (It == Prepared.end() && Prepared.size() >= Limits.MaxPreparedBlocks)
		{
			const auto Oldest = std::min_element(Prepared.begin(), Prepared.end(),
			                                     [](const auto& InA, const auto& InB)
			                                     {
				                                     return InA.second.Access < InB.second.Access;
			                                     });
			Prepared.erase(Oldest);
			++Stats.Evictions;
		}
		Prepared.insert_or_assign(Key, std::move(Block));
		return Result;
	}

	FBufferSlice Bind(const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
	                  const FMaterialParameterSchema& InSchema, const FResolvedMaterialParameters& InParameters)
	{
		++Stats.FullLookups;
		const auto Hash = HashBinding(InBinding, InFormat, InSchema, InParameters);
		const auto Bucket = Entries.find(Hash);
		if (Bucket != Entries.end())
		{
			for (auto& Existing : Bucket->second)
			{
				if (!Existing.IsExpired() && MatchesBinding(Existing.Key, InBinding, InFormat, InSchema, InParameters))
				{
					++Stats.Reuses;
					Recent.splice(Recent.end(), Recent, Existing.Recency);
					return Existing.Slice;
				}
			}
		}
		FCacheEntry Entry = MakeEntry(InBinding, InFormat, InSchema, InParameters);
		if (TrackScope)
		{
			for (const auto& OwnerScope : Entry.Owners)
			{
				TrackScope(OwnerScope.lock());
			}
		}
		bool bTransient = false;
		for (const auto& [Scope, Key] : Entry.Key.Scopes)
		{
			bTransient |= Scope == EMaterialScope::Frame || Scope == EMaterialScope::Draw ||
			              Scope == EMaterialScope::View || Scope == EMaterialScope::Pass;
			++Stats.ScopePacks[static_cast<std::size_t>(Scope)];
		}
		const auto Data = PackMaterialConstants(InBinding, InParameters.Values);
		if (Data.size() > PageSize)
		{
			throw std::invalid_argument("Material constant block exceeds page capacity");
		}
		Entry.Slice = Publish(Data, bTransient);
		++Stats.Packs;
		const auto Result = Entry.Slice;
		// Replace history under the complete logical key, even while the producer keeps that key alive.
		if (Bucket != Entries.end())
		{
			std::erase_if(Bucket->second,
			              [&](const FCacheEntry& InPrevious)
			              {
				              if (!MatchesBinding(InPrevious.Key, InBinding, InFormat, InSchema, InParameters, false))
				              {
					              return false;
				              }
				              Untrack(InPrevious);
				              ++Stats.Evictions;
				              return true;
			              });
			if (Bucket->second.empty())
			{
				Entries.erase(Bucket);
			}
		}
		if (Entry.Slice.Extent <= Limits.MaxBytes && Limits.MaxBlocks != 0)
		{
			Store(Hash, std::move(Entry));
		}
		return Result;
	}
};

FMaterialConstantCache::FMaterialConstantCache(IRHIDevice& InDevice, std::uint32_t InPageSize,
                                               FMaterialConstantLimits InLimits)
    : Impl(std::make_unique<FImpl>(InDevice, InPageSize, InLimits))
{
	const auto Alignment = InDevice.GetCapabilities().ConstantAlignment;
	if (Alignment == 0 || InPageSize == 0 || InPageSize % Alignment != 0)
	{
		throw std::invalid_argument("Invalid material constant page alignment or capacity");
	}
}

FMaterialConstantCache::~FMaterialConstantCache() = default;

void FMaterialConstantCache::SetScopeTracker(std::function<void(const std::shared_ptr<const void>&)> InTracker)
{
	Impl->TrackScope = std::move(InTracker);
}

std::vector<FConstantBinding> FMaterialConstantCache::Bind(const FCompiledMaterialPass& InPass,
                                                           const FMaterialParameterSchema& InSchema,
                                                           const FResolvedMaterialParameters& InParameters)
{
	HYP_PERF_SCOPE_C(Detail, BindMaterialConstants);
	Impl->CheckOwner();
	std::vector<FConstantBinding> Result;
	for (std::uint32_t Index = 0; Index < InPass.Bindings.size(); ++Index)
	{
		const auto& Binding = InPass.Bindings[Index];
		if (Binding.Resource.Kind == EBindingKind::UniformBuffer)
		{
			Result.push_back({Index, Impl->Bind(Binding, InPass.Vertex.Format, InSchema, InParameters)});
		}
	}
	return Result;
}

std::vector<FConstantBinding> FMaterialConstantCache::BindPrepared(
    std::shared_ptr<const FCompiledMaterialDefinition> InProgram, const FCompiledMaterialPass& InPass,
    const FResolvedMaterialParameters& InParameters, FMaterialConstantState& InState)
{
	HYP_PERF_SCOPE_C(Detail, BindMaterialConstants);
	Impl->CheckOwner();
	if (!InProgram || &InProgram->GetPass(InPass.Usage, InPass.Variant) != &InPass)
	{
		throw std::invalid_argument("Prepared constants require an owned immutable program pass");
	}
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		const auto& Input = InParameters.Scopes[Scope];
		if ((InParameters.DependenciesMask & (1U << Scope)) &&
		    (!Input.Lifetime || !Input.Key.Identity || !Input.Key.Revision))
		{
			throw std::invalid_argument("Prepared material requires an owned scope identity and revision");
		}
	}
	const bool bSameProgram = InState.Pass == &InPass && InState.Program.lock() == InProgram;
	std::vector<FConstantBinding> Result;
	std::size_t ConstantIndex{};
	for (std::uint32_t Index = 0; Index < InPass.Bindings.size(); ++Index)
	{
		const auto& Binding = InPass.Bindings[Index];
		if (Binding.Resource.Kind != EBindingKind::UniformBuffer)
		{
			continue;
		}
		bool bSame = bSameProgram && ConstantIndex < InState.Bindings.size();
		for (const auto& Member : Binding.Members)
		{
			if (!bSame)
			{
				break;
			}
			bSame = InState.Values[Member.ParameterIndex] == InParameters.Values[Member.ParameterIndex];
		}
		if (bSame)
		{
			Result.push_back(InState.Bindings[ConstantIndex]);
			++Impl->Stats.Reuses;
			++Impl->Stats.PreparedReuses;
		}
		else
		{
			Result.push_back({Index, Impl->BindShared(InProgram, Binding, InPass.Vertex.Format, InParameters)});
		}
		++ConstantIndex;
	}
	FMaterialConstantState Next{InProgram, &InPass, InParameters.Values, Result};
	InState = std::move(Next);
	return Result;
}

std::vector<FConstantBinding> FMaterialConstantCache::BindInstances(std::shared_ptr<const FInstanceBatchData> InData)
{
	Impl->CheckOwner();
	if (!InData || !InData->InstanceCount || InData->Constants.empty())
	{
		throw std::invalid_argument("Missing instance constant data");
	}
	const auto Key = InData.get();
	const auto Existing = Impl->Instances.find(Key);
	if (Existing != Impl->Instances.end())
	{
		if (Existing->second.Data.lock() == InData && InData->IsLive())
		{
			Impl->RecentInstances.splice(Impl->RecentInstances.end(), Impl->RecentInstances, Existing->second.Recent);
			Impl->Stats.InstanceReuses += Existing->second.Bindings.size();
			return Existing->second.Bindings;
		}
		Impl->EraseInstance(Existing);
	}
	FImpl::FInstanceEntry Entry;
	Entry.Data = InData;
	if (Impl->TrackScope)
	{
		for (const auto& Owner : InData->Owners)
		{
			Impl->TrackScope(Owner.lock());
		}
	}
	for (const auto& Block : InData->Constants)
	{
		auto Slice = Impl->Publish(Block.Bytes, false);
		Entry.Bytes += Slice.Extent;
		Entry.Bindings.push_back({Block.Slot, std::move(Slice)});
		Impl->Stats.InstanceUploadBytes += Block.Bytes.size();
		++Impl->Stats.Packs;
	}
	const auto Result = Entry.Bindings;
	if (InData->IsLive() && Impl->Limits.MaxPreparedBlocks && Entry.Bytes <= Impl->Limits.MaxBytes)
	{
		while (!Impl->RecentInstances.empty() && (Impl->Instances.size() >= Impl->Limits.MaxPreparedBlocks ||
		                                          Impl->InstanceBytes + Entry.Bytes > Impl->Limits.MaxBytes))
		{
			Impl->EraseInstance(Impl->Instances.find(Impl->RecentInstances.front()));
			++Impl->Stats.Evictions;
		}
		Impl->RecentInstances.push_back(Key);
		Entry.Recent = std::prev(Impl->RecentInstances.end());
		Impl->InstanceBytes += Entry.Bytes;
		Impl->Instances.emplace(Key, std::move(Entry));
	}
	return Result;
}

bool FMaterialConstantCache::Collect()
{
	Impl->CheckOwner();
	for (auto It = Impl->Instances.begin(); It != Impl->Instances.end();)
	{
		const auto Data = It->second.Data.lock();
		if (!Data || !Data->IsLive())
		{
			const auto Previous = It++;
			Impl->EraseInstance(Previous);
		}
		else
		{
			++It;
		}
	}
	for (auto Iterator = Impl->Entries.begin(); Iterator != Impl->Entries.end();)
	{
		std::erase_if(Iterator->second,
		              [&](const FCacheEntry& InEntry)
		              {
			              if (!InEntry.IsExpired())
			              {
				              return false;
			              }
			              Impl->Untrack(InEntry);
			              return true;
		              });
		if (Iterator->second.empty())
		{
			Iterator = Impl->Entries.erase(Iterator);
		}
		else
		{
			++Iterator;
		}
	}
	std::erase_if(Impl->Prepared,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Program.expired();
	              });
	bool bKeptIdle = false;
	std::erase_if(Impl->Pages,
	              [&](FImpl::FPage& InPage)
	              {
		              if (InPage.Buffer.Payload.use_count() != 1)
		              {
			              return false;
		              }
		              if (bKeptIdle)
		              {
			              return true;
		              }
		              bKeptIdle = true;
		              if (InPage.End != 0)
		              {
			              Impl->Device.ResetConstantBuffer(InPage.Buffer);
			              InPage.End = 0;
			              ++Impl->Stats.PagesReset;
		              }
		              return false;
	              });
	std::set<const IRHIBuffer*> UsedPages;
	for (const auto& [Hash, Bucket] : Impl->Entries)
	{
		for (const auto& Entry : Bucket)
		{
			UsedPages.insert(Entry.Slice.Buffer.Payload.get());
		}
	}
	for (const auto& [Key, Block] : Impl->Prepared)
	{
		UsedPages.insert(Block.Slice.Buffer.Payload.get());
	}
	for (const auto& [Key, Entry] : Impl->Instances)
	{
		for (const auto& Binding : Entry.Bindings)
		{
			UsedPages.insert(Binding.Slice.Buffer.Payload.get());
		}
	}
	for (const auto& Page : Impl->Pages)
	{
		// Live scopes will notify on retirement. Poll only pages whose CPU entries are gone but packets remain.
		if (!UsedPages.contains(Page.Buffer.Payload.get()) && Page.Buffer.Payload.use_count() > 1)
		{
			return true;
		}
	}
	return false;
}

void FMaterialConstantCache::Clear()
{
	Impl->CheckOwner();
	Impl->Recent.clear();
	Impl->Entries.clear();
	Impl->Prepared.clear();
	Impl->Instances.clear();
	Impl->RecentInstances.clear();
	Impl->InstanceBytes = 0;
	Impl->Stats.CachedBytes = 0;
	Impl->Stats.CachedBlocks = 0;
	Collect();
}

bool FMaterialConstantCache::CanRelease() const
{
	Impl->CheckOwner();
	return std::all_of(Impl->Pages.begin(), Impl->Pages.end(),
	                   [](const FImpl::FPage& InPage)
	                   {
		                   return InPage.Buffer.Payload.use_count() == 1;
	                   });
}

FMaterialConstantStats FMaterialConstantCache::Statistics() const
{
	Impl->CheckOwner();
	auto Result = Impl->Stats;
	Result.LivePages = Impl->Pages.size();
	Result.PreparedBlocks = Impl->Prepared.size();
	Result.InstanceBlocks = Impl->Instances.size();
	Result.InstanceBytes = Impl->InstanceBytes;
	Result.PageBytes = Impl->Pages.size() * Impl->PageSize;
	return Result;
}
} // namespace Hyperion
