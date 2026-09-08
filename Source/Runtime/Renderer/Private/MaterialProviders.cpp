#include "Hyperion/Renderer/MaterialProviders.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <set>
#include <stdexcept>

namespace Hyperion
{
const FMaterialValue* FMaterialProviderInputs::Find(EMaterialScope InScope, std::string_view InName) const
{
	for (const auto& Entry : Values.at(static_cast<std::size_t>(InScope)))
	{
		if (Entry.Name == InName)
		{
			return &Entry.Value;
		}
	}
	return nullptr;
}

struct FMaterialProviderRegistry::FImpl
{
	struct FEntry
	{
		std::vector<FMaterialScopeKey> Keys;
		std::vector<FMaterialParameterValues> Inputs;
		std::vector<std::weak_ptr<const void>> Owners;
		std::optional<FMaterialValue> Value;

		bool IsExpired() const
		{
			return std::any_of(Owners.begin(), Owners.end(),
			                   [](const auto& InOwner)
			                   {
				                   return InOwner.expired();
			                   });
		}
	};

	std::shared_ptr<const FMaterialSemanticRegistry> Semantics;
	std::map<std::string, FMaterialProviderDescription> Providers;
	std::map<std::pair<std::string, std::uint64_t>, std::vector<FEntry>> Cache;
	std::uint64_t Version = 1;
	std::atomic<bool> bFrozen{};
	FMaterialProviderStats Stats;

	const FEntry* FindDefault(const FMaterialProviderDescription& InProvider,
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
				return nullptr;
			}
			Hash = (Hash ^ Input.Key.Identity) * 1099511628211ULL;
			Hash = (Hash ^ Input.Key.Revision) * 1099511628211ULL;
			for (const auto Value : Input.Key.Qualifiers)
			{
				Hash = (Hash ^ Value) * 1099511628211ULL;
			}
		}
		const auto Bucket = Cache.find({InProvider.Semantic, Hash});
		if (Bucket == Cache.end())
		{
			return nullptr;
		}
		for (const auto& Entry : Bucket->second)
		{
			bool bMatches = !Entry.IsExpired();
			std::size_t Index{};
			for (std::size_t Scope = 0; bMatches && Scope < MaterialScopeCount; ++Scope)
			{
				if ((InProvider.Dependencies & (1U << Scope)) == 0)
				{
					continue;
				}
				bMatches = Entry.Keys[Index] == InInputs.Scopes[Scope].Key;
				std::size_t Count{};
				for (const auto& Value : InInputs.Values[Scope])
				{
					if (Value.Name == InProvider.Semantic)
					{
						++Count;
						bMatches &= Entry.Inputs[Index].size() == 1 && Entry.Inputs[Index].front().Value == Value.Value;
					}
				}
				bMatches &= Count == Entry.Inputs[Index].size();
				++Index;
			}
			if (bMatches)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	std::optional<FEntry> Inputs(const FMaterialProviderDescription& InProvider,
	                             const FMaterialProviderInputs& InInputs, bool bInDefault) const
	{
		FEntry Result;
		for (std::size_t Index = 0; Index < MaterialScopeCount; ++Index)
		{
			if ((InProvider.Dependencies & (1U << Index)) == 0)
			{
				continue;
			}
			const auto& Input = InInputs.Scopes[Index];
			if (!Input.Lifetime)
			{
				return {};
			}
			if (Input.Key.Identity == 0 || Input.Key.Revision == 0)
			{
				throw std::invalid_argument("Material provider input requires a valid scope key");
			}
			Result.Keys.push_back(Input.Key);
			Result.Inputs.emplace_back();
			auto& Values = Result.Inputs.back();
			for (const auto& Value : InInputs.Values[Index])
			{
				// Builtin providers read one named value; custom callbacks can read all declared scopes.
				if (!bInDefault || Value.Name == InProvider.Semantic)
				{
					Values.push_back(Value);
				}
			}
			std::sort(Values.begin(), Values.end(),
			          [](const auto& InA, const auto& InB)
			          {
				          return InA.Name < InB.Name;
			          });
			std::string Previous;
			for (const auto& Value : Values)
			{
				if (Value.Name.empty() || Value.Name == Previous)
				{
					throw std::invalid_argument("Duplicate or empty material provider input");
				}
				Value.Value.Validate();
				Previous = Value.Name;
			}
			Result.Owners.push_back(Input.Lifetime);
		}
		return Result;
	}

	FMaterialProvidedValue Evaluate(const FMaterialProviderDescription& InProvider,
	                                const FMaterialProviderInputs& InInputs, bool bInDefault)
	{
		if (bInDefault)
		{
			if (const auto* Existing = FindDefault(InProvider, InInputs))
			{
				++Stats.Reuses;
				return {InProvider.Semantic, Existing->Value, InProvider.Dependencies};
			}
		}
		auto Entry = Inputs(InProvider, InInputs, bInDefault);
		if (!Entry)
		{
			return {InProvider.Semantic, {}, InProvider.Dependencies};
		}
		std::uint64_t Hash = 14695981039346656037ULL;
		for (const auto& Key : Entry->Keys)
		{
			Hash = (Hash ^ Key.Identity) * 1099511628211ULL;
			Hash = (Hash ^ Key.Revision) * 1099511628211ULL;
			for (const auto Value : Key.Qualifiers)
			{
				Hash = (Hash ^ Value) * 1099511628211ULL;
			}
		}
		auto& Bucket = Cache[{InProvider.Semantic, Hash}];
		for (const auto& Existing : Bucket)
		{
			if (!Existing.IsExpired() && Existing.Keys == Entry->Keys && Existing.Inputs == Entry->Inputs)
			{
				++Stats.Reuses;
				return {InProvider.Semantic, Existing.Value, InProvider.Dependencies};
			}
		}
		FMaterialProviderInputs DeclaredInputs;
		for (std::size_t Index = 0; Index < MaterialScopeCount; ++Index)
		{
			if ((InProvider.Dependencies & (1U << Index)) != 0)
			{
				DeclaredInputs.Scopes[Index] = InInputs.Scopes[Index];
				DeclaredInputs.Values[Index] = InInputs.Values[Index];
			}
		}
		Entry->Value = InProvider.Evaluate(DeclaredInputs);
		const auto& Semantic = Semantics->Find(InProvider.Semantic);
		if (Entry->Value)
		{
			Entry->Value->Validate();
			if (Entry->Value->Type != Semantic.Type)
			{
				throw std::invalid_argument("Material provider result type mismatch: " + Semantic.Name);
			}
		}
		++Stats.Evaluations[static_cast<std::size_t>(Semantic.Scope)];
		FMaterialProvidedValue Result{InProvider.Semantic, Entry->Value, InProvider.Dependencies};
		const auto Previous = std::find_if(Bucket.begin(), Bucket.end(),
		                                   [&](const FEntry& InEntry)
		                                   {
			                                   return InEntry.Keys == Entry->Keys;
		                                   });
		if (Previous != Bucket.end())
		{
			// One current value per complete scope key. Old frozen draws own their resolved values.
			*Previous = std::move(*Entry);
		}
		else
		{
			Bucket.push_back(std::move(*Entry));
			++Stats.CachedEntries;
		}
		return Result;
	}
};

FMaterialProviderRegistry::FMaterialProviderRegistry(std::shared_ptr<const FMaterialSemanticRegistry> InSemantics)
    : Impl(std::make_unique<FImpl>())
{
	if (!InSemantics)
	{
		throw std::invalid_argument("Material providers require a semantic registry");
	}
	auto Registry = std::make_shared<FMaterialSemanticRegistry>(*InSemantics);
	Registry->Freeze();
	Impl->Semantics = std::move(Registry);
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
		const auto Custom = Impl->Providers.find(Semantic.Name);
		FMaterialProviderDescription Provider;
		if (Custom != Impl->Providers.end())
		{
			Provider = Custom->second;
		}
		else
		{
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
		}
		Result.push_back(Impl->Evaluate(Provider, InInputs, Custom == Impl->Providers.end()));
	}
	return Result;
}

void FMaterialProviderRegistry::Collect()
{
	for (auto It = Impl->Cache.begin(); It != Impl->Cache.end();)
	{
		Impl->Stats.CachedEntries -= std::erase_if(It->second,
		                                           [](const FImpl::FEntry& InEntry)
		                                           {
			                                           return InEntry.IsExpired();
		                                           });
		if (It->second.empty())
		{
			It = Impl->Cache.erase(It);
		}
		else
		{
			++It;
		}
	}
}

FMaterialProviderStats FMaterialProviderRegistry::Statistics() const
{
	return Impl->Stats;
}
} // namespace Hyperion
