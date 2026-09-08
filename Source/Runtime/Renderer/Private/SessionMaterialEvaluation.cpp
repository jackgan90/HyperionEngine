#include "Hyperion/Core/Profiling.h"
#include "MaterialEvaluationCache.h"
#include "SessionMaterialsInternal.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
constexpr auto ScopeIndex(EMaterialScope InScope)
{
	return static_cast<std::size_t>(InScope);
}

std::uint32_t ChangedEngineScopes(const FMaterialEvaluationCache::FEntry& InEntry,
                                  const FMaterialProviderInputs& InInputs)
{
	std::uint32_t Changed{};
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if (Scope != ScopeIndex(EMaterialScope::Object) && Scope != ScopeIndex(EMaterialScope::Material) &&
		    (InEntry.Dependencies & (1U << Scope)) &&
		    (InEntry.Inputs.Scopes[Scope].Key != InInputs.Scopes[Scope].Key ||
		     InEntry.Inputs.Values[Scope] != InInputs.Values[Scope]))
		{
			Changed |= 1U << Scope;
		}
	}
	return Changed;
}

void CommitEvaluation(FMaterialEvaluationCache::FEntry& InEntry, const FMaterialProviderInputs& InInputs,
                      std::uint32_t InChanged, FResolvedMaterialParameters InValues,
                      const FCompiledMaterialPass& InPass)
{
	struct FScopeUpdate
	{
		std::size_t Index{};
		FMaterialScopeInput Scope;
		FMaterialParameterValues Values;
	};

	std::vector<FScopeUpdate> Updates;
	std::uint32_t Dependencies{};
	for (const auto Index : InPass.ActiveParameters)
	{
		Dependencies |= InValues.Dependencies[Index];
	}
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((InChanged & (1U << Scope)) != 0)
		{
			InValues.Scopes[Scope] = InInputs.Scopes[Scope];
			Updates.push_back({Scope, InInputs.Scopes[Scope], InInputs.Values[Scope]});
		}
	}
	if ((Dependencies & MaterialScopeBit(EMaterialScope::Material)) != 0)
	{
		InValues.Scopes[ScopeIndex(EMaterialScope::Material)] = {
		    {InEntry.Snapshot->Identity, InEntry.Snapshot->Revision}, InEntry.Snapshot};
	}
	auto Resolved = std::make_shared<const FResolvedMaterialParameters>(std::move(InValues));
	// All allocating and validating work finishes before replacing the last valid cached evaluation.
	for (auto& Update : Updates)
	{
		InEntry.Inputs.Scopes[Update.Index] = std::move(Update.Scope);
		InEntry.Inputs.Values[Update.Index] = std::move(Update.Values);
	}
	InEntry.Dependencies = Dependencies;
	InEntry.Resolved = std::move(Resolved);
}
} // namespace

bool RefreshMaterialEvaluation(FRenderItem& InItem, const FRenderView& InView, const FMaterialProviderInputs& InInputs,
                               const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled,
                               FMaterialProviderRegistry& InProviders)
{
	HYP_PERF_SCOPE_C(Detail, RefreshMaterialEvaluation);
	if (!InItem.EvaluationCache || !InItem.LocalItemId)
	{
		return false;
	}
	const auto It = InItem.EvaluationCache->Entries.find({*InItem.LocalItemId, InView.Identity});
	if (It == InItem.EvaluationCache->Entries.end())
	{
		return false;
	}
	auto& Entry = It->second;
	// Only refresh engine scopes when the compiled program, overrides and emitted object are unchanged.
	// Transient Draw/Pass/Frame consumers retain the complete validation path in Matches.
	if (Entry.Snapshot != InItem.State.Surface->GetSnapshot() || Entry.Compiled != InCompiled ||
	    Entry.Usage != InView.Usage ||
	    !Entry.Matches(InInputs, InItem.Context.ObjectParameters, InItem.DrawParameters, InItem.State.World,
	                   InItem.State.bClipSpace, InItem.State.ObjectInputs, ~0U))
	{
		return false;
	}
	const auto Changed = ChangedEngineScopes(Entry, InInputs);
	const auto& Parameters = InCompiled->Interface.Schema->GetParameters();
	std::vector<std::size_t> Indices;
	std::vector<std::string> Semantics;
	for (const auto Index : InCompiled->GetPass(InView.Usage).ActiveParameters)
	{
		if ((Entry.Resolved->Dependencies[Index] & Changed) != 0)
		{
			// A provider mixing Object and View may read derived WVP; rebuild those object inputs on the full path.
			if (Parameters[Index].Source != EMaterialParameterSource::Semantic ||
			    (Entry.Resolved->Dependencies[Index] & MaterialScopeBit(EMaterialScope::Object)) != 0)
			{
				return false;
			}
			Indices.push_back(Index);
			Semantics.push_back(Parameters[Index].Semantic);
		}
	}
	const auto Providers = InProviders.Evaluate(InInputs, Semantics);
	FResolvedMaterialParameters Values = *Entry.Resolved;
	for (const auto Index : Indices)
	{
		const auto& Parameter = Parameters[Index];
		const auto Provider = std::find_if(Providers.begin(), Providers.end(),
		                                   [&](const auto& InProvider)
		                                   {
			                                   return InProvider.Semantic == Parameter.Semantic;
		                                   });
		if (Provider == Providers.end())
		{
			return false;
		}
		const auto& Value = Provider->Value ? Provider->Value : Parameter.Default;
		Values.Values[Index] = Value ? std::make_shared<const FMaterialValue>(*Value) : nullptr;
		Values.Dependencies[Index] =
		    Provider->Dependencies | (Provider->Value ? 0U : MaterialScopeBit(EMaterialScope::Material));
		if ((!Values.Values[Index] && Parameter.bRequired) ||
		    (Values.Values[Index] && Values.Values[Index]->Type != Parameter.Type))
		{
			throw std::invalid_argument("Missing or incompatible material input: " + Parameter.Name);
		}
	}
	for (const auto& Binding : InCompiled->GetPass(InView.Usage).Bindings)
	{
		if (Binding.ResourceParameter && (Entry.Resolved->Dependencies[*Binding.ResourceParameter] & Changed) != 0)
		{
			Values.ResourceIdentity = std::make_shared<const int>(0);
			break;
		}
	}
	CommitEvaluation(Entry, InInputs, Changed, std::move(Values), InCompiled->GetPass(InView.Usage));
	InItem.ResolvedParameters = Entry.Resolved;
	return true;
}
} // namespace Hyperion
