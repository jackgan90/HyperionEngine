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
		     InEntry.Inputs.Scopes[Scope].Lifetime != InInputs.Scopes[Scope].Lifetime ||
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
	HYP_PERF_SCOPE_C(Detail, CommitMaterialEvaluation);
	std::uint32_t Dependencies{};
	for (const auto Index : InPass.ActiveParameters)
	{
		Dependencies |= InValues.Dependencies[Index];
	}
	InValues.DependenciesMask = Dependencies;
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((InChanged & (1U << Scope)) != 0)
		{
			InValues.Scopes[Scope] = InInputs.Scopes[Scope];
		}
		if ((Dependencies & (1U << Scope)) == 0)
		{
			InValues.Scopes[Scope] = {};
		}
	}
	if ((Dependencies & MaterialScopeBit(EMaterialScope::Material)) != 0)
	{
		InValues.Scopes[ScopeIndex(EMaterialScope::Material)] = {
		    {InEntry.Snapshot->Identity, InEntry.Snapshot->Revision}, InEntry.Snapshot};
	}
	auto Resolved = std::make_shared<const FResolvedMaterialParameters>(std::move(InValues));
	// All allocating and validating work finishes before replacing the last valid cached evaluation.
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((Dependencies & (1U << Scope)) == 0)
		{
			InEntry.Inputs.Scopes[Scope] = {};
			InEntry.Inputs.Values[Scope] = {};
		}
		else if ((InChanged & (1U << Scope)) != 0)
		{
			InEntry.Inputs.Scopes[Scope] = InInputs.Scopes[Scope];
			InEntry.Inputs.Values[Scope] = InInputs.Values[Scope];
		}
	}
	InEntry.Dependencies = Dependencies;
	InEntry.Resolved = std::move(Resolved);
}

struct FMaterialInputRefresh
{
	const FMaterialProviderInputs& Base;
	std::optional<FMaterialProviderInputs> Derived;
	std::uint32_t Changed{};
	bool bObjectChanged{};

	const FMaterialProviderInputs& Get() const
	{
		return Derived ? *Derived : Base;
	}
};

FMaterialInputRefresh PrepareInputRefresh(const FMaterialEvaluationCache::FEntry& InEntry, const FRenderItem& InItem,
                                          const FRenderSceneSnapshot& InSnapshot,
                                          const FMaterialProviderInputs& InBaseInputs,
                                          const FRenderResourceService& InResources)
{
	FMaterialInputRefresh Result{InBaseInputs};
	if ((InEntry.Dependencies & MaterialScopeBit(EMaterialScope::Draw)) != 0)
	{
		Result.Derived.emplace(InBaseInputs);
		FillMaterialDrawInputs(*Result.Derived, InItem, InSnapshot);
	}
	Result.Changed = ChangedEngineScopes(InEntry, Result.Get());
	Result.bObjectChanged = std::bit_cast<std::array<std::uint32_t, 16>>(InEntry.World.Values) !=
	                            std::bit_cast<std::array<std::uint32_t, 16>>(InItem.State.World.Values) ||
	                        InEntry.bClipSpace != InItem.State.bClipSpace ||
	                        InEntry.ObjectInputs != InItem.State.ObjectInputs;
	if (Result.bObjectChanged)
	{
		Result.Changed |= MaterialScopeBit(EMaterialScope::Object);
	}
	bool bNeedsObject = false;
	for (const auto Index : InEntry.ProviderParameters)
	{
		bNeedsObject |= (InEntry.Resolved->Dependencies[Index] & Result.Changed) != 0 &&
		                (InEntry.Resolved->Dependencies[Index] & MaterialScopeBit(EMaterialScope::Object)) != 0;
	}
	if (bNeedsObject || (Result.bObjectChanged && (InEntry.Dependencies & MaterialScopeBit(EMaterialScope::Object))))
	{
		if (!Result.Derived)
		{
			Result.Derived.emplace(InBaseInputs);
		}
		FillMaterialObjectInputs(*Result.Derived, InItem, InSnapshot, InResources);
	}
	return Result;
}

FResolvedMaterialParameters EvaluateChangedParameters(const FMaterialEvaluationCache::FEntry& InEntry,
                                                      const FMaterialParameterSchema& InSchema,
                                                      const FCompiledMaterialPass& InPass,
                                                      const FMaterialProviderInputs& InInputs, std::uint32_t InChanged,
                                                      FMaterialProviderRegistry& InProviders)
{
	HYP_PERF_SCOPE_C(Detail, EvaluateChangedParameters);
	const auto& Parameters = InSchema.GetParameters();
	FResolvedMaterialParameters Values = *InEntry.Resolved;
	for (const auto Index : InEntry.ProviderParameters)
	{
		if ((InEntry.Resolved->Dependencies[Index] & InChanged) == 0)
		{
			continue;
		}
		const auto& Parameter = Parameters[Index];
		const auto Provider = InProviders.EvaluateOne(InInputs, Parameter.Semantic);
		const auto Value =
		    Provider.Value ? Provider.Value.Share()
		                   : (Parameter.Default ? std::make_shared<const FMaterialValue>(*Parameter.Default) : nullptr);
		if (!SameMaterialValue(Values.Values[Index], Value))
		{
			Values.Values.Set(Index, Value);
		}
		Values.Dependencies.Set(Index, Provider.Dependencies |
		                                   (Provider.Value ? 0U : MaterialScopeBit(EMaterialScope::Material)));
		if ((!Values.Values[Index] && Parameter.bRequired) ||
		    (Values.Values[Index] && Values.Values[Index]->Type != Parameter.Type))
		{
			throw std::invalid_argument("Missing or incompatible material input: " + Parameter.Name);
		}
	}
	for (const auto& Binding : InPass.Bindings)
	{
		if (Binding.ResourceParameter && (InEntry.Resolved->Dependencies[*Binding.ResourceParameter] & InChanged) != 0)
		{
			Values.ResourceIdentity = std::make_shared<const int>(0);
			break;
		}
	}
	return Values;
}

} // namespace

bool RefreshMaterialEvaluation(FRenderItem& InItem, const FRenderSceneSnapshot& InSnapshot,
                               const FMaterialProviderInputs& InBaseInputs, const FRenderResourceService& InResources,
                               const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled,
                               FMaterialProviderRegistry& InProviders)
{
	HYP_PERF_SCOPE_C(Detail, RefreshMaterialEvaluation);
	const auto& View = InSnapshot.View;
	if (!InItem.EvaluationCache || !InItem.LocalItemId)
	{
		return false;
	}
	const auto It = InItem.EvaluationCache->Entries.find({*InItem.LocalItemId, View.Identity});
	if (It == InItem.EvaluationCache->Entries.end())
	{
		return false;
	}
	auto& Entry = It->second;
	// Authoring/program changes use complete validation. Scope changes use incremental evaluation.
	if (Entry.Snapshot != InItem.State.Surface->GetSnapshot() || Entry.Compiled != InCompiled ||
	    Entry.Usage != View.Usage ||
	    !Entry.Matches(InBaseInputs, InItem.Context.ObjectParameters, InItem.DrawParameters, InItem.State.World,
	                   InItem.State.bClipSpace, InItem.State.ObjectInputs, ~0U))
	{
		return false;
	}
	const auto Refresh = PrepareInputRefresh(Entry, InItem, InSnapshot, InBaseInputs, InResources);
	const auto& Pass = InCompiled->GetPass(View.Usage);
	auto Values = EvaluateChangedParameters(Entry, *InCompiled->Interface.Schema, Pass, Refresh.Get(), Refresh.Changed,
	                                        InProviders);
	std::optional<FMaterialParameterValues> ObjectInputs;
	if (Refresh.bObjectChanged)
	{
		ObjectInputs = InItem.State.ObjectInputs;
	}
	CommitEvaluation(Entry, Refresh.Get(), Refresh.Changed, std::move(Values), Pass);
	Entry.World = InItem.State.World;
	Entry.AccessFrame = InSnapshot.Frame->Frame;
	InItem.EvaluationCache->TouchObject(*InItem.LocalItemId, Entry.AccessFrame);
	Entry.bClipSpace = InItem.State.bClipSpace;
	if (ObjectInputs)
	{
		Entry.ObjectInputs = std::move(*ObjectInputs);
	}
	InItem.ResolvedParameters = Entry.Resolved;
	return true;
}
} // namespace Hyperion
