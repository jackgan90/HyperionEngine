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
		    (InEntry.Inputs->Scopes[Scope].Key != InInputs.Scopes[Scope].Key ||
		     InEntry.Inputs->Scopes[Scope].Lifetime != InInputs.Scopes[Scope].Lifetime ||
		     InEntry.Inputs->Values[Scope] != InInputs.Values[Scope]))
		{
			Changed |= 1U << Scope;
		}
	}
	return Changed;
}

void CommitEvaluation(FMaterialEvaluationCache::FEntry& InEntry, const FMaterialProviderInputs& InInputs,
                      std::uint32_t InChanged, FResolvedMaterialParameters InValues,
                      const FCompiledMaterialPass& InPass, FViewMaterialProviders& InProviders)
{
	HYP_PERF_SCOPE_C(Detail, CommitMaterialEvaluation);
	std::uint32_t Dependencies{};
	InValues.LocalDependenciesMask = 0;
	for (const auto Index : InPass.ActiveParameters)
	{
		Dependencies |= InValues.Dependencies[Index];
		if (std::find(InEntry.SharedProviderParameters.begin(), InEntry.SharedProviderParameters.end(), Index) ==
		    InEntry.SharedProviderParameters.end())
		{
			InValues.LocalDependenciesMask |= InValues.Dependencies[Index];
		}
	}
	InValues.DependenciesMask = Dependencies;
	const auto Inputs = InProviders.RetainInputs(InInputs, Dependencies);
	for (const auto Scope : {EMaterialScope::Material, EMaterialScope::Object, EMaterialScope::Draw})
	{
		const auto Index = ScopeIndex(Scope);
		const auto Bit = MaterialScopeBit(Scope);
		if ((Dependencies & Bit) == 0 && InValues.Scopes[Index].Lifetime)
		{
			InValues.Scopes.Set(Index, {});
		}
		else if (InChanged & Dependencies & Bit)
		{
			InValues.Scopes.Set(Index, InInputs.Scopes[Index]);
		}
	}
	if ((Dependencies & MaterialScopeBit(EMaterialScope::Material)) != 0 &&
	    !InValues.Scopes[ScopeIndex(EMaterialScope::Material)].Lifetime)
	{
		InValues.Scopes.Set(ScopeIndex(EMaterialScope::Material),
		                    {{InEntry.Snapshot->Identity, InEntry.Snapshot->Revision}, InEntry.Snapshot});
	}
	InValues.Scopes.ShareEngine({Inputs, &Inputs->Scopes});
	auto Resolved = std::make_shared<const FResolvedMaterialParameters>(std::move(InValues));
	// All allocating and validating work finishes before replacing the last valid cached evaluation.
	InEntry.Inputs = Inputs;
	InEntry.Dependencies = Dependencies;
	InEntry.Resolved = std::move(Resolved);
	InEntry.Shared.reset();
	PrepareSharedMaterialEligibility(InEntry);
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
                                                      FViewMaterialProviders& InProviders)
{
	HYP_PERF_SCOPE_C(Detail, EvaluateChangedParameters);
	const auto& Parameters = InSchema.GetParameters();
	FResolvedMaterialParameters Values =
	    InEntry.Shared ? ComposeMaterialParameters(*InEntry.Resolved, *InEntry.Shared) : *InEntry.Resolved;
	if (!InEntry.SharedProviderParameters.empty())
	{
		const auto& Shared = InProviders.PrepareShared(InEntry, InPass, InInputs);
		Values.Values.SetShared(Shared.Values);
		Values.Dependencies.SetShared(Shared.Dependencies);
	}
	for (const auto Index : InEntry.ProviderParameters)
	{
		if ((InEntry.Resolved->Dependencies[Index] & InChanged) == 0)
		{
			continue;
		}
		const auto& Parameter = Parameters[Index];
		const auto Provider = InProviders.Evaluate(InInputs, Parameter.Semantic);
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
		if (Binding.ResourceParameter && !SameMaterialValue(Values.Values[*Binding.ResourceParameter],
		                                                    InEntry.Resolved->Values[*Binding.ResourceParameter]))
		{
			// A camera/light numeric update does not change the textures or samplers in this descriptor table.
			Values.ResourceIdentity = std::make_shared<const int>(0);
			break;
		}
	}
	return Values;
}

} // namespace

std::shared_ptr<const FMaterialProviderInputs> FViewMaterialProviders::RetainInputs(
    const FMaterialProviderInputs& InInputs, std::uint32_t InDependencies)
{
	constexpr auto PerItem = MaterialScopeBit(EMaterialScope::Material) | MaterialScopeBit(EMaterialScope::Object);
	const auto Dependencies = InDependencies & ~PerItem;
	const bool bShared = (Dependencies & MaterialScopeBit(EMaterialScope::Draw)) == 0;
	if (bShared)
	{
		if (const auto Found = RetainedInputs.find(Dependencies); Found != RetainedInputs.end())
		{
			return Found->second;
		}
	}
	auto Result = std::make_shared<FMaterialProviderInputs>();
	for (std::size_t Index = 0; Index < MaterialScopeCount; ++Index)
	{
		if (Dependencies & (1U << Index))
		{
			Result->Scopes[Index] = InInputs.Scopes[Index];
			Result->Values[Index] = InInputs.Values[Index];
		}
	}
	if (bShared)
	{
		RetainedInputs.emplace(Dependencies, Result);
	}
	return Result;
}

const FViewMaterialProviders::FRefresh& FViewMaterialProviders::PrepareShared(
    const FMaterialEvaluationCache::FEntry& InEntry, const FCompiledMaterialPass& InPass,
    const FMaterialProviderInputs& InInputs)
{
	HYP_PERF_SCOPE_C(Detail, SharedMaterialRefresh);
	if (InEntry.SharedGroup)
	{
		if (const auto Found = GroupRefreshes.find(InEntry.SharedGroup.get()); Found != GroupRefreshes.end())
		{
			return Found->second;
		}
	}
	const FRefreshLookup Key{&InPass, InEntry.SharedProviderParameters};
	if (const auto Found = Refreshes.find(Key); Found != Refreshes.end())
	{
		if (InEntry.SharedGroup && GroupRefreshes.size() < 128)
		{
			auto Result = Found->second;
			Result.Group = InEntry.SharedGroup;
			return GroupRefreshes.emplace(InEntry.SharedGroup.get(), std::move(Result)).first->second;
		}
		return Found->second;
	}
	const auto& Parameters = InEntry.Compiled->Interface.Schema->GetParameters();
	std::vector<std::optional<std::shared_ptr<const FMaterialValue>>> Values(Parameters.size());
	std::vector<std::optional<std::uint32_t>> Dependencies(Parameters.size());
	std::uint32_t SharedMask{};
	bool bProvided = true;
	for (const auto Index : InEntry.SharedProviderParameters)
	{
		const auto& Parameter = Parameters[Index];
		const auto Provider = Evaluate(InInputs, Parameter.Semantic);
		bProvided &= bool(Provider.Value);
		const auto Value =
		    Provider.Value ? Provider.Value.Share()
		                   : (Parameter.Default ? std::make_shared<const FMaterialValue>(*Parameter.Default) : nullptr);
		if ((!Value && Parameter.bRequired) || (Value && Value->Type != Parameter.Type))
		{
			throw std::invalid_argument("Missing or incompatible material input: " + Parameter.Name);
		}
		// Preserve immutable value identities for unchanged values across scope revisions.
		Values[Index] =
		    SameMaterialValue(InEntry.Resolved->Values[Index], Value) ? InEntry.Resolved->Values[Index] : Value;
		Dependencies[Index] =
		    Provider.Dependencies | (Provider.Value ? 0U : MaterialScopeBit(EMaterialScope::Material));
		SharedMask |= *Dependencies[Index];
	}
	FRefresh Result{std::make_shared<const FMaterialValueTable::FSharedValues>(std::move(Values)),
	                std::make_shared<const FMaterialDependencyTable::FSharedValues>(std::move(Dependencies))};
	Result.Group = InEntry.SharedGroup
	                   ? InEntry.SharedGroup
	                   : std::make_shared<const std::vector<std::size_t>>(InEntry.SharedProviderParameters);
	if (bProvided)
	{
		const auto Inputs = RetainInputs(InInputs, SharedMask);
		Result.Parameters = std::make_shared<const FMaterialSharedParameters>(
		    FMaterialSharedParameters{Result.Values, Result.Dependencies, {Inputs, &Inputs->Scopes}, SharedMask});
	}
	// Temporary per-view history is bounded; published overlays live only as long as their current items/frames.
	if (Refreshes.size() < 128)
	{
		Refreshes.emplace(FRefreshKey{&InPass, InEntry.SharedProviderParameters}, Result);
		const auto* Group = Result.Group.get();
		return GroupRefreshes.emplace(Group, std::move(Result)).first->second;
	}
	UncachedRefresh = std::move(Result);
	return UncachedRefresh;
}

FMaterialProvidedValue FViewMaterialProviders::Evaluate(const FMaterialProviderInputs& InInputs,
                                                        std::string_view InSemantic)
{
	if (const auto Found = Shared.find(InSemantic); Found != Shared.end())
	{
		return Found->second;
	}
	auto Result = Registry.EvaluateOne(InInputs, InSemantic);
	constexpr auto PerItem = MaterialScopeBit(EMaterialScope::Object) | MaterialScopeBit(EMaterialScope::Material) |
	                         MaterialScopeBit(EMaterialScope::Draw);
	if ((Result.Dependencies & PerItem) == 0)
	{
		Shared.emplace(InSemantic, Result);
	}
	return Result;
}

bool RefreshMaterialEvaluation(FRenderItem& InItem, const FRenderSceneSnapshot& InSnapshot,
                               const FMaterialProviderInputs& InBaseInputs, const FRenderResourceService& InResources,
                               const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled,
                               FViewMaterialProviders& InProviders)
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
	CommitEvaluation(Entry, Refresh.Get(), Refresh.Changed, std::move(Values), Pass, InProviders);
	Entry.World = InItem.State.World;
	Entry.AccessFrame = InSnapshot.Frame->Frame;
	InItem.EvaluationCache->TouchObject(*InItem.LocalItemId, Entry.AccessFrame);
	Entry.bClipSpace = InItem.State.bClipSpace;
	if (ObjectInputs)
	{
		Entry.ObjectInputs = std::move(*ObjectInputs);
	}
	InItem.ResolvedParameters = Entry.Resolved;
	InItem.SharedParameters.reset();
	Entry.SharedBinding = InProviders.RetainSharedBinding(Entry);
	InItem.SharedBinding = Entry.SharedBinding;
	return true;
}
} // namespace Hyperion
