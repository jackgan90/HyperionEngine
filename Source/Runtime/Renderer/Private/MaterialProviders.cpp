#include "Hyperion/Renderer/MaterialProviders.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <atomic>
#include <list>
#include <map>
#include <set>
#include <stdexcept>

namespace Hyperion
{
const FMaterialValue* FMaterialProviderInputs::Find(EMaterialScope InScope, std::string_view InName) const
{
	const auto& Entries = Values.at(static_cast<std::size_t>(InScope)).Get();
	const auto It = std::lower_bound(Entries.begin(), Entries.end(), InName,
	                                 [](const auto& InEntry, std::string_view InKey)
	                                 {
		                                 return InEntry.Name < InKey;
	                                 });
	return It != Entries.end() && It->Name == InName ? &It->Value : nullptr;
}

struct FMaterialProviderRegistry::FImpl
{
	struct FEntry
	{
		std::vector<FMaterialScopeKey> Keys;
		std::vector<FMaterialInputValues> Inputs;
		std::vector<std::weak_ptr<const void>> Owners;
		std::vector<std::weak_ptr<const FMaterialParameterValues>> Sources;
		std::size_t Bytes{};
		FMaterialSharedValue Value;
		const std::pair<std::string, std::uint64_t>* BucketKey{};
		std::list<FEntry>::iterator Location;
		std::list<FEntry*>::iterator Recency;

		bool IsExpired() const
		{
			for (std::size_t Index = 0; Index < Owners.size(); ++Index)
			{
				if (Owners[Index].expired())
				{
					return true;
				}
			}
			return false;
		}
	};

	std::shared_ptr<const FMaterialSemanticRegistry> Semantics;
	std::map<std::string, FMaterialProviderDescription, std::less<>> Providers;
	std::map<std::string, FMaterialProviderDescription, std::less<>> Defaults;
	std::map<std::pair<std::string, std::uint64_t>, std::list<FEntry>> Cache;
	std::list<FEntry*> Recent;
	std::uint64_t Version = 1;
	std::atomic<bool> bFrozen{};
	FMaterialProviderStats Stats;
	FMaterialProviderLimits Limits;

	bool Matches(const FEntry& InEntry, const FMaterialProviderDescription& InProvider,
	             const FMaterialProviderInputs& InInputs, bool bInDefault) const
	{
		if (InEntry.IsExpired())
		{
			return false;
		}
		std::size_t Index{};
		for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
		{
			if ((InProvider.Dependencies & (1U << Scope)) == 0)
			{
				continue;
			}
			if (InEntry.Keys[Index] != InInputs.Scopes[Scope].Key)
			{
				return false;
			}
			const auto& Previous = InEntry.Inputs[Index];
			const auto Source = InEntry.Sources[Index++].lock();
			const auto& Current = InInputs.Values[Scope];
			if ((Source && Source == Current.Share()) || Previous == Current)
			{
				continue;
			}
			if (!bInDefault)
			{
				return false;
			}
			// Default providers read only their semantic; unrelated values must not invalidate them.
			const FMaterialValue* OldValue{};
			for (const auto& Entry : Previous.Get())
			{
				if (Entry.Name == InProvider.Semantic)
				{
					OldValue = &Entry.Value;
					break;
				}
			}
			const auto* NewValue = InInputs.Find(static_cast<EMaterialScope>(Scope), InProvider.Semantic);
			if ((!OldValue != !NewValue) || (OldValue && *OldValue != *NewValue))
			{
				return false;
			}
		}
		return true;
	}

	void Untrack(const FEntry& InEntry)
	{
		Recent.erase(InEntry.Recency);
		Stats.CachedValueBytes -= InEntry.Bytes;
		--Stats.CachedEntries;
	}

	void Trim(std::size_t InBytes)
	{
		while (!Recent.empty() &&
		       (Stats.CachedEntries >= Limits.MaxEntries || Stats.CachedValueBytes + InBytes > Limits.MaxValueBytes))
		{
			const auto* Entry = Recent.front();
			const auto Bucket = Cache.find(*Entry->BucketKey);
			const auto Location = Entry->Location;
			Untrack(*Entry);
			Bucket->second.erase(Location);
			++Stats.Evictions;
			if (Bucket->second.empty())
			{
				Cache.erase(Bucket);
			}
		}
	}

	void Store(const std::pair<std::string, std::uint64_t>& InKey, FEntry InEntry)
	{
		const auto Bucket = Cache.find(InKey);
		if (Bucket != Cache.end())
		{
			std::erase_if(Bucket->second,
			              [&](const FEntry& InPrevious)
			              {
				              if (InPrevious.Keys != InEntry.Keys)
				              {
					              return false;
				              }
				              Untrack(InPrevious);
				              ++Stats.Evictions;
				              return true;
			              });
			if (Bucket->second.empty())
			{
				Cache.erase(Bucket);
			}
		}
		if (Limits.MaxEntries == 0 || InEntry.Bytes > Limits.MaxValueBytes)
		{
			return;
		}
		Trim(InEntry.Bytes);
		auto StoredBucket = Cache.try_emplace(InKey).first;
		auto& Entries = StoredBucket->second;
		auto Location = Entries.end();
		try
		{
			Location = Entries.insert(Entries.end(), std::move(InEntry));
			Recent.push_back(&*Location);
		}
		catch (...)
		{
			if (Location != Entries.end())
			{
				Entries.erase(Location);
			}
			if (Entries.empty())
			{
				Cache.erase(StoredBucket);
			}
			throw;
		}
		auto& Stored = *Location;
		Stored.BucketKey = &StoredBucket->first;
		Stored.Location = Location;
		Stored.Recency = std::prev(Recent.end());
		Stats.CachedValueBytes += Stored.Bytes;
		++Stats.CachedEntries;
	}

	std::optional<std::uint64_t> HashInputs(const FMaterialProviderDescription& InProvider,
	                                        const FMaterialProviderInputs& InInputs) const
	{
		std::uint64_t Hash = 14695981039346656037ULL;
		for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
		{
			if ((InProvider.Dependencies & (1U << Scope)) == 0)
			{
				continue;
			}
			const auto& Input = InInputs.Scopes[Scope];
			if (!Input.Lifetime)
			{
				return {};
			}
			if (!Input.Key.Identity || !Input.Key.Revision)
			{
				throw std::invalid_argument("Material provider input requires a valid scope key");
			}
			Hash = (Hash ^ Input.Key.Identity) * 1099511628211ULL;
			Hash = (Hash ^ Input.Key.Revision) * 1099511628211ULL;
			for (const auto Value : Input.Key.GetQualifiers())
			{
				Hash = (Hash ^ Value) * 1099511628211ULL;
			}
		}
		return Hash;
	}

	FEntry PrepareEntry(const FMaterialProviderDescription& InProvider, const FMaterialProviderInputs& InInputs,
	                    bool bInDefault)
	{
		FEntry Entry;
		const auto& Semantic = Semantics->Find(InProvider.Semantic);
		FMaterialProviderInputs DeclaredInputs;
		for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
		{
			if ((InProvider.Dependencies & (1U << Scope)) == 0)
			{
				continue;
			}
			Entry.Keys.push_back(InInputs.Scopes[Scope].Key);
			Entry.Sources.push_back(InInputs.Values[Scope].Share());
			if (bInDefault)
			{
				const auto* Value = InInputs.Find(static_cast<EMaterialScope>(Scope), InProvider.Semantic);
				Entry.Inputs.push_back(Value ? FMaterialInputValues{{InProvider.Semantic, *Value}}
				                             : FMaterialInputValues{});
				if (Value && Scope == static_cast<std::size_t>(Semantic.Scope))
				{
					// The selected input was validated on publication. Own only this single semantic, so unrelated
					// textures/buffers in the original scope are neither copied nor retained by its numeric provider.
					const auto& Selected = Entry.Inputs.back();
					Entry.Value = FMaterialSharedValue(
					    std::shared_ptr<const FMaterialValue>(Selected.Share(), &Selected.Get().front().Value));
				}
			}
			else
			{
				DeclaredInputs.Scopes[Scope] = InInputs.Scopes[Scope];
				DeclaredInputs.Values[Scope] = InInputs.Values[Scope];
				Entry.Inputs.push_back(InInputs.Values[Scope]);
			}
			Entry.Bytes += Entry.Inputs.back().GetStorageBytes() +
			               InInputs.Scopes[Scope].Key.GetQualifiers().capacity() * sizeof(std::uint64_t);
			Entry.Owners.push_back(InInputs.Scopes[Scope].Lifetime);
		}
		if (!bInDefault)
		{
			Entry.Value = InProvider.Evaluate(DeclaredInputs);
		}
		if (Entry.Value)
		{
			if (!bInDefault)
			{
				Entry.Value->Validate();
			}
			if (Entry.Value->Type != Semantic.Type)
			{
				throw std::invalid_argument("Material provider result type mismatch: " + Semantic.Name);
			}
		}
		Entry.Bytes += sizeof(FEntry) + (!bInDefault && Entry.Value ? MaterialValueStorageBytes(*Entry.Value) : 0);
		return Entry;
	}

	FMaterialProvidedValue Evaluate(const FMaterialProviderDescription& InProvider,
	                                const FMaterialProviderInputs& InInputs, bool bInDefault)
	{
		const auto Hash = HashInputs(InProvider, InInputs);
		if (!Hash)
		{
			return {InProvider.Semantic, {}, InProvider.Dependencies};
		}
		const auto CacheKey = std::pair{InProvider.Semantic, *Hash};
		const auto Bucket = Cache.find(CacheKey);
		if (Bucket != Cache.end())
		{
			for (auto& Existing : Bucket->second)
			{
				if (Matches(Existing, InProvider, InInputs, bInDefault))
				{
					++Stats.Reuses;
					Recent.splice(Recent.end(), Recent, Existing.Recency);
					return {InProvider.Semantic, Existing.Value, InProvider.Dependencies};
				}
			}
		}
		auto Entry = PrepareEntry(InProvider, InInputs, bInDefault);
		++Stats.Evaluations[static_cast<std::size_t>(Semantics->Find(InProvider.Semantic).Scope)];
		FMaterialProvidedValue Result{InProvider.Semantic, Entry.Value, InProvider.Dependencies};
		Store(CacheKey, std::move(Entry));
		return Result;
	}
};

FMaterialProviderRegistry::FMaterialProviderRegistry(std::shared_ptr<const FMaterialSemanticRegistry> InSemantics,
                                                     FMaterialProviderLimits InLimits)
    : Impl(std::make_unique<FImpl>())
{
	if (!InSemantics)
	{
		throw std::invalid_argument("Material providers require a semantic registry");
	}
	auto Registry = std::make_shared<FMaterialSemanticRegistry>(*InSemantics);
	Registry->Freeze();
	Impl->Semantics = std::move(Registry);
	Impl->Limits = InLimits;
}

FMaterialProviderRegistry::~FMaterialProviderRegistry() = default;

void FMaterialProviderRegistry::Register(FMaterialProviderDescription InDescription)
{
	const auto& Semantic = Impl->Semantics->Find(InDescription.Semantic);
	if (Impl->bFrozen || !InDescription.Evaluate || InDescription.Dependencies == 0 ||
	    (InDescription.Dependencies >> MaterialScopeCount) != 0 ||
	    (InDescription.Dependencies & MaterialScopeBit(Semantic.Scope)) == 0 || Impl->Providers.contains(Semantic.Name))
	{
		throw std::invalid_argument("Invalid, duplicate or frozen material provider registration");
	}
	InDescription.Semantic = Semantic.Name;
	Impl->Providers.emplace(Semantic.Name, std::move(InDescription));
	++Impl->Version;
}

void FMaterialProviderRegistry::Freeze()
{
	Impl->bFrozen = true;
}

std::uint64_t FMaterialProviderRegistry::GetVersion() const
{
	return Impl->Version;
}

std::vector<FMaterialProvidedValue> FMaterialProviderRegistry::Evaluate(const FMaterialProviderInputs& InInputs,
                                                                        std::span<const std::string> InSemantics)
{
	if (!Impl->bFrozen)
	{
		throw std::logic_error("Freeze material providers before preparing a frame");
	}
	std::vector<FMaterialProvidedValue> Result;
	std::set<std::string> Seen;
	for (const auto& Requested : InSemantics)
	{
		const auto& Semantic = Impl->Semantics->Find(Requested);
		if (!Seen.insert(Semantic.Name).second)
		{
			continue;
		}
		Result.push_back(EvaluateOne(InInputs, Semantic.Name));
	}
	return Result;
}

FMaterialProvidedValue FMaterialProviderRegistry::EvaluateOne(const FMaterialProviderInputs& InInputs,
                                                              std::string_view InSemantic)
{
	if (!Impl->bFrozen)
	{
		throw std::logic_error("Freeze material providers before preparing a frame");
	}
	const auto& Semantic = Impl->Semantics->Find(InSemantic);
	const auto Custom = Impl->Providers.find(Semantic.Name);
	if (Custom != Impl->Providers.end())
	{
		return Impl->Evaluate(Custom->second, InInputs, false);
	}
	auto Existing = Impl->Defaults.find(Semantic.Name);
	if (Existing == Impl->Defaults.end())
	{
		FMaterialProviderDescription Provider;
		Provider.Semantic = Semantic.Name;
		Provider.Dependencies = MaterialScopeBit(Semantic.Scope);
		if (Semantic.Name == "Engine.Object.WorldViewProjection")
		{
			Provider.Dependencies |= MaterialScopeBit(EMaterialScope::View);
		}
		Provider.Evaluate = [Scope = Semantic.Scope, Name = Semantic.Name](
		                        const FMaterialProviderInputs& InValues) -> std::optional<FMaterialValue>
		{
			const auto* Value = InValues.Find(Scope, Name);
			return Value ? std::optional<FMaterialValue>(*Value) : std::nullopt;
		};
		Existing = Impl->Defaults.emplace(Semantic.Name, std::move(Provider)).first;
	}
	return Impl->Evaluate(Existing->second, InInputs, true);
}

void FMaterialProviderRegistry::Collect()
{
	HYP_PERF_SCOPE_C(Material, CollectMaterialProviders);
	// Walk the existing entry index once. Bucket lookup is needed only for an actual retirement.
	for (auto It = Impl->Recent.begin(); It != Impl->Recent.end();)
	{
		const auto* Entry = *It++;
		if (!Entry->IsExpired())
		{
			continue;
		}
		const auto Bucket = Impl->Cache.find(*Entry->BucketKey);
		const auto Location = Entry->Location;
		Impl->Untrack(*Entry);
		Bucket->second.erase(Location);
		if (Bucket->second.empty())
		{
			Impl->Cache.erase(Bucket);
		}
	}
}

FMaterialProviderStats FMaterialProviderRegistry::Statistics() const
{
	return Impl->Stats;
}
} // namespace Hyperion
