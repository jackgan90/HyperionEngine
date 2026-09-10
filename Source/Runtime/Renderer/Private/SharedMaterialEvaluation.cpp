#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "MaterialEvaluationCache.h"
#include <algorithm>

namespace Hyperion
{
void PrepareSharedMaterialEligibility(FMaterialEvaluationCache::FEntry& InEntry)
{
	InEntry.SharedBinding.reset();
	InEntry.SharedResources.clear();
	InEntry.bSeparateShared =
	    !InEntry.SharedProviderParameters.empty() && !(InEntry.Dependencies & MaterialScopeBit(EMaterialScope::Draw));
	for (const auto Index : InEntry.ProviderParameters)
	{
		InEntry.bSeparateShared &= !(InEntry.Resolved->Dependencies[Index] & FMaterialResolvedScopes::EngineMask);
	}
	const auto Inspect = [&](const FCompiledMaterialPass& InPass)
	{
		for (const auto& Binding : InPass.Bindings)
		{
			if (Binding.ResourceParameter &&
			    std::find(InEntry.SharedProviderParameters.begin(), InEntry.SharedProviderParameters.end(),
			              *Binding.ResourceParameter) != InEntry.SharedProviderParameters.end())
			{
				InEntry.SharedResources.push_back(*Binding.ResourceParameter);
			}
			if (Binding.InstanceStride)
			{
				for (const auto& Member : Binding.Members)
				{
					InEntry.bSeparateShared &=
					    std::find(InEntry.SharedProviderParameters.begin(), InEntry.SharedProviderParameters.end(),
					              Member.ParameterIndex) == InEntry.SharedProviderParameters.end();
				}
			}
		}
	};
	Inspect(InEntry.Compiled->GetPass(InEntry.Usage));
	if (const auto* Instance = InEntry.Compiled->FindInstancePass(InEntry.Usage))
	{
		Inspect(*Instance);
	}
}

bool ShareMaterialEvaluation(FRenderItem& InItem, const FRenderSceneSnapshot& InSnapshot,
                             const FMaterialProviderInputs& InInputs,
                             const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled,
                             FViewMaterialProviders& InProviders)
{
	HYP_PERF_SCOPE_C(Detail, ShareMaterialEvaluation);
	if (!InItem.EvaluationCache || !InItem.LocalItemId)
	{
		return false;
	}
	const auto Found = InItem.EvaluationCache->Entries.find({*InItem.LocalItemId, InSnapshot.View.Identity});
	if (Found == InItem.EvaluationCache->Entries.end())
	{
		return false;
	}
	auto& Entry = Found->second;
	const bool bSameLocal = InItem.Preparation && Entry.Preparation.lock() == InItem.Preparation;
	if (!Entry.bSeparateShared || Entry.Snapshot != InItem.State.Surface->GetSnapshot() ||
	    Entry.Compiled != InCompiled || Entry.Usage != InSnapshot.View.Usage ||
	    (!bSameLocal &&
	     !Entry.Matches(InInputs, InItem.Context.ObjectParameters, InItem.DrawParameters, InItem.State.World,
	                    InItem.State.bClipSpace, InItem.State.ObjectInputs, FMaterialResolvedScopes::EngineMask)))
	{
		return false;
	}
	const auto& Update = InProviders.PrepareShared(Entry, InCompiled->GetPass(Entry.Usage), InInputs);
	if (!Update.Parameters)
	{
		return false; // Missing/default transitions retain the complete dependency and material-owner path.
	}
	const auto Base = Entry.Resolved->Values.GetSharedIdentity();
	if (!InProviders.ValidatedSharedBases.contains(Base))
	{
		for (const auto Index : Entry.SharedProviderParameters)
		{
			if (Entry.Resolved->Dependencies[Index] != *Update.Dependencies->Get(Index))
			{
				return false;
			}
		}
		for (const auto Index : Entry.SharedResources)
		{
			if (!SameMaterialValue(Entry.Resolved->Values[Index], *Update.Values->Get(Index)))
			{
				return false;
			}
		}
		if (Base && InProviders.ValidatedSharedBases.size() < 128)
		{
			InProviders.ValidatedSharedBases.insert(Base);
		}
	}
	Entry.Inputs = InProviders.RetainInputs(InInputs, Entry.Dependencies);
	Entry.Shared = Update.Parameters;
	Entry.AccessFrame = InSnapshot.Frame->Frame;
	InItem.EvaluationCache->TouchObject(*InItem.LocalItemId, Entry.AccessFrame);
	InItem.ResolvedParameters = Entry.Resolved;
	InItem.SharedParameters = Entry.Shared;
	Entry.SharedBinding = InProviders.RetainSharedBinding(Entry);
	InItem.SharedBinding = Entry.SharedBinding;
	return true;
}
} // namespace Hyperion
