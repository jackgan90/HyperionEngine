#include "MaterialSharedBinding.h"
#include "Hyperion/Core/Profiling.h"
#include "MaterialEvaluationCache.h"

namespace Hyperion
{
namespace
{
bool SameBinding(const FMaterialSharedBinding& InA, const FMaterialSharedBinding& InB)
{
	if (InA.Program != InB.Program || InA.Pass != InB.Pass || InA.Parameters != InB.Parameters ||
	    InA.Dependencies != InB.Dependencies || InA.Resources.size() != InB.Resources.size())
	{
		return false;
	}
	for (std::size_t Index = 0; Index < InA.Resources.size(); ++Index)
	{
		const auto& A = InA.Resources[Index];
		const auto& B = InB.Resources[Index];
		const auto Value = A.Value.lock();
		const auto Other = B.Value.lock();
		if (A.Index != B.Index || !Value || !Other || !SameMaterialValue(Value, Other))
		{
			return false;
		}
	}
	return true;
}

std::shared_ptr<const FMaterialSharedBinding> RetainBindingHistory(
    std::shared_ptr<const FMaterialSharedBinding> InBinding,
    std::vector<std::weak_ptr<const FMaterialSharedBinding>>* InHistory)
{
	if (!InHistory)
	{
		return InBinding;
	}
	for (std::size_t Index = 0; Index < InHistory->size(); ++Index)
	{
		if (auto Existing = (*InHistory)[Index].lock(); Existing && SameBinding(*Existing, *InBinding))
		{
			return Existing;
		}
	}
	std::erase_if(*InHistory,
	              [](const auto& InEntry)
	              {
		              return InEntry.expired();
	              });
	if (InHistory->size() < 128)
	{
		InHistory->push_back(InBinding);
	}
	return InBinding;
}

FViewMaterialProviders::FRefresh BuildBindingRefresh(const FMaterialSharedBinding& InBinding,
                                                     const FMaterialProviderInputs& InInputs,
                                                     FViewMaterialProviders& InProviders)
{
	const auto& Parameters = InBinding.Program->Interface.Schema->GetParameters();
	std::vector<std::optional<std::shared_ptr<const FMaterialValue>>> Values(Parameters.size());
	std::vector<std::optional<std::uint32_t>> Dependencies(Parameters.size());
	std::uint32_t Mask{};
	for (std::size_t Position = 0; Position < InBinding.Parameters.size(); ++Position)
	{
		const auto Index = InBinding.Parameters[Position];
		const auto& Parameter = Parameters[Index];
		const auto Provided = InProviders.Evaluate(InInputs, Parameter.Semantic);
		if (!Provided.Value || Provided.Dependencies != InBinding.Dependencies[Position] ||
		    Provided.Value->Type != Parameter.Type)
		{
			return {}; // Default/missing/dependency transitions require complete evaluation and its diagnostics.
		}
		Values[Index] = Provided.Value.Share();
		Dependencies[Index] = Provided.Dependencies;
		Mask |= Provided.Dependencies;
	}
	const auto Inputs = InProviders.RetainInputs(InInputs, Mask);
	FViewMaterialProviders::FRefresh Result{
	    std::make_shared<const FMaterialValueTable::FSharedValues>(std::move(Values)),
	    std::make_shared<const FMaterialDependencyTable::FSharedValues>(std::move(Dependencies))};
	Result.Parameters = std::make_shared<const FMaterialSharedParameters>(
	    FMaterialSharedParameters{Result.Values, Result.Dependencies, {Inputs, &Inputs->Scopes}, Mask});
	Result.Group = std::make_shared<const std::vector<std::size_t>>(InBinding.Parameters);
	return Result;
}

std::shared_ptr<const FMaterialSharedParameters> RefreshBinding(const FMaterialSharedBinding& InBinding,
                                                                const FMaterialProviderInputs& InInputs,
                                                                FViewMaterialProviders& InProviders)
{
	HYP_PERF_SCOPE_C(Detail, RefreshSharedMaterialGroup);
	const FViewMaterialProviders::FRefreshLookup Key{InBinding.Pass, InBinding.Parameters};
	auto Found = InProviders.Refreshes.find(Key);
	FViewMaterialProviders::FRefresh Uncached;
	if (Found == InProviders.Refreshes.end())
	{
		Uncached = BuildBindingRefresh(InBinding, InInputs, InProviders);
		if (Uncached.Parameters && InProviders.Refreshes.size() < 128)
		{
			Found = InProviders.Refreshes
			            .emplace(FViewMaterialProviders::FRefreshKey{InBinding.Pass, InBinding.Parameters},
			                     std::move(Uncached))
			            .first;
		}
	}
	const auto& Refresh = Found == InProviders.Refreshes.end() ? Uncached : Found->second;
	if (!Refresh.Parameters)
	{
		return {};
	}
	for (std::size_t Position = 0; Position < InBinding.Parameters.size(); ++Position)
	{
		if (*Refresh.Dependencies->Get(InBinding.Parameters[Position]) != InBinding.Dependencies[Position])
		{
			return {};
		}
	}
	for (const auto& Resource : InBinding.Resources)
	{
		const auto Previous = Resource.Value.lock();
		if (!Previous || !SameMaterialValue(Previous, *Refresh.Values->Get(Resource.Index)))
		{
			return {};
		}
	}
	return Refresh.Parameters;
}
} // namespace

std::shared_ptr<const FMaterialSharedBinding> FViewMaterialProviders::RetainSharedBinding(
    const FMaterialEvaluationCache::FEntry& InEntry)
{
	if (!InEntry.bSeparateShared)
	{
		return {};
	}
	if (InEntry.SharedBinding)
	{
		return InEntry.SharedBinding;
	}
	const auto& Pass = InEntry.Compiled->GetPass(InEntry.Usage);
	const FBindingKey Key{&Pass, InEntry.Resolved->Values.GetSharedIdentity(),
	                      InEntry.Resolved->Dependencies.GetSharedIdentity()};
	if (const auto Found = SharedBindings.find(Key); Found != SharedBindings.end())
	{
		return Found->second;
	}
	auto Result = std::make_shared<FMaterialSharedBinding>();
	Result->Program = InEntry.Compiled;
	Result->Pass = &Pass;
	Result->Parameters = InEntry.SharedProviderParameters;
	for (const auto Index : Result->Parameters)
	{
		Result->Dependencies.push_back(InEntry.Resolved->Dependencies[Index]);
	}
	for (const auto Index : InEntry.SharedResources)
	{
		Result->Resources.push_back({Index, InEntry.Resolved->Values[Index]});
	}
	const auto Retained = RetainBindingHistory(std::move(Result), BindingHistory);
	if (SharedBindings.size() < 128)
	{
		SharedBindings.emplace(Key, Retained);
	}
	return Retained;
}

const std::shared_ptr<const FMaterialSharedParameters>& FViewMaterialProviders::UpdateSharedBinding(
    const std::shared_ptr<const FMaterialSharedBinding>& InBinding, const FMaterialProviderInputs& InInputs)
{
	// The usual view has a handful of contracts. A dense table avoids per-item tree lookup and semantic discovery.
	for (const auto& Update : BindingUpdates)
	{
		if (Update.first == InBinding)
		{
			return Update.second;
		}
	}
	++BindingEvaluations;
	auto Update = RefreshBinding(*InBinding, InInputs, *this);
	if (BindingUpdates.size() < 128)
	{
		return BindingUpdates.emplace_back(InBinding, std::move(Update)).second;
	}
	UncachedBindingUpdate = std::move(Update);
	return UncachedBindingUpdate;
}
} // namespace Hyperion
