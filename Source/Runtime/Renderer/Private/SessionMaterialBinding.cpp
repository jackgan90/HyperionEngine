#include "Hyperion/Core/Profiling.h"
#include "MaterialBindingGroups.h"
#include "MaterialEvaluationCache.h"
#include "MaterialProfiling.h"
#include "MaterialSharedBinding.h"
#include "SceneItemPreparation.h"
#include "SessionMaterialsInternal.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace Hyperion
{
namespace
{
constexpr auto ScopeIndex(EMaterialScope InScope)
{
	return static_cast<std::size_t>(InScope);
}

FMaterialScopeKey ItemKey(const FRenderItem& InItem, const FRenderSceneSnapshot& InSnapshot)
{
	std::vector<std::uint64_t> Qualifiers{InItem.Primitive.Slot, InItem.Primitive.Generation,
	                                      InItem.LocalItemId.value_or(InItem.Ordinal)};
	if (!InItem.LocalItemId)
	{
		Qualifiers.insert(Qualifiers.end(),
		                  {InSnapshot.Frame->Frame, InSnapshot.Family, InSnapshot.View.Identity, InItem.Ordinal});
	}
	for (const auto Value : InItem.State.World.Values)
	{
		Qualifiers.push_back(std::bit_cast<std::uint32_t>(Value));
	}
	Qualifiers.push_back(InItem.State.bClipSpace);
	FMaterialScopeKey Key{InItem.Primitive.Scene, 1, std::move(Qualifiers)};
	return Key;
}

std::shared_ptr<const void> ObjectLifetime(const FRenderItem& InItem, const FRenderSceneSnapshot& InSnapshot,
                                           const FMaterialScopeKey& InKey, const FRenderResourceService& InResources)
{
	const auto FrameLifetime = InSnapshot.Frame->Inputs.Scopes[ScopeIndex(EMaterialScope::Frame)].Lifetime;
	if (!InItem.EvaluationCache || !InItem.LocalItemId)
	{
		return FrameLifetime;
	}
	auto& Objects = InItem.EvaluationCache->Objects;
	if (!Objects.contains(*InItem.LocalItemId) && Objects.size() >= 64)
	{
		const auto Oldest = std::min_element(Objects.begin(), Objects.end(),
		                                     [](const auto& InA, const auto& InB)
		                                     {
			                                     return InA.second.AccessFrame < InB.second.AccessFrame;
		                                     });
		if (Oldest->second.AccessFrame == InSnapshot.Frame->Frame)
		{
			// Preserve this frame's hot set when a repeated collection exceeds capacity.
			return FrameLifetime;
		}
		Objects.erase(Oldest);
	}
	auto& Entry = Objects[*InItem.LocalItemId];
	if (!Entry.Scope.Lifetime || Entry.Scope.Key != InKey || Entry.Inputs != InItem.State.ObjectInputs ||
	    Entry.Overrides != InItem.Context.ObjectParameters)
	{
		// Retire previous emitted contents even when the primitive's publication revision is unchanged.
		Entry = {
		    {InKey, InResources.CreateScopeLifetime()}, InItem.State.ObjectInputs, InItem.Context.ObjectParameters};
	}
	Entry.AccessFrame = InSnapshot.Frame->Frame;
	return Entry.Scope.Lifetime;
}

bool ReuseEvaluation(FRenderItem& InItem, const FRenderView& InView, const FMaterialProviderInputs& InInputs,
                     const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled)
{
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
	const bool bSameLocal = InItem.Preparation && Entry.Preparation.lock() == InItem.Preparation;
	if (Entry.Snapshot != InItem.State.Surface->GetSnapshot() || Entry.Compiled != InCompiled ||
	    Entry.Usage != InView.Usage ||
	    !(bSameLocal ? Entry.MatchesEngine(InInputs)
	                 : Entry.Matches(InInputs, InItem.Context.ObjectParameters, InItem.DrawParameters,
	                                 InItem.State.World, InItem.State.bClipSpace, InItem.State.ObjectInputs)))
	{
		return false;
	}
	InItem.ResolvedParameters = Entry.Resolved;
	InItem.SharedParameters = Entry.Shared;
	InItem.SharedBinding = Entry.SharedBinding;
	Entry.AccessFrame = InInputs.Scopes[ScopeIndex(EMaterialScope::Frame)].Key.Revision;
	InItem.EvaluationCache->TouchObject(*InItem.LocalItemId, Entry.AccessFrame);
	return true;
}

void ClassifyMaterialProviders(FMaterialEvaluationCache::FEntry& InEntry)
{
	std::vector<bool> Overridden(InEntry.Compiled->Interface.Schema->GetParameters().size());
	const std::array<const FMaterialParameterValues*, 3> OverrideSets{&InEntry.Snapshot->Overrides, &InEntry.Object,
	                                                                  &InEntry.Draw};
	for (const auto* Overrides : OverrideSets)
	{
		for (const auto& Override : *Overrides)
		{
			Overridden[InEntry.Compiled->Interface.Schema->Find(Override.Name).Index] = true;
		}
	}
	for (const auto Index : InEntry.Compiled->GetPass(InEntry.Usage).ActiveParameters)
	{
		InEntry.Dependencies |= InEntry.Resolved->Dependencies[Index];
		if (!Overridden[Index] &&
		    InEntry.Compiled->Interface.Schema->GetParameters()[Index].Source == EMaterialParameterSource::Semantic)
		{
			constexpr auto PerItem = MaterialScopeBit(EMaterialScope::Object) |
			                         MaterialScopeBit(EMaterialScope::Material) |
			                         MaterialScopeBit(EMaterialScope::Draw);
			if ((InEntry.Resolved->Dependencies[Index] & PerItem) == 0)
			{
				InEntry.SharedProviderParameters.push_back(Index);
			}
			else
			{
				InEntry.ProviderParameters.push_back(Index);
			}
		}
	}
}

void CacheEvaluation(FRenderItem& InItem, const FRenderView& InView, const FMaterialProviderInputs& InInputs,
                     const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled,
                     FViewMaterialProviders& InProviders)
{
	if (!InItem.EvaluationCache || !InItem.LocalItemId)
	{
		return;
	}
	const auto Key = std::pair{*InItem.LocalItemId, InView.Identity};
	auto& Entries = InItem.EvaluationCache->Entries;
	if (!Entries.contains(Key) && Entries.size() >= 64)
	{
		const auto Oldest = std::min_element(Entries.begin(), Entries.end(),
		                                     [](const auto& InA, const auto& InB)
		                                     {
			                                     return InA.second.AccessFrame < InB.second.AccessFrame;
		                                     });
		if (Oldest->second.AccessFrame == InInputs.Scopes[ScopeIndex(EMaterialScope::Frame)].Key.Revision)
		{
			// Newly scanned overflow items must not evict entries already used this frame.
			return;
		}
		Entries.erase(Oldest);
	}
	FMaterialEvaluationCache::FEntry Entry;
	Entry.AccessFrame = InInputs.Scopes[ScopeIndex(EMaterialScope::Frame)].Key.Revision;
	Entry.Snapshot = InItem.State.Surface->GetSnapshot();
	Entry.Compiled = InCompiled;
	Entry.Usage = InView.Usage;
	Entry.Object = InItem.Context.ObjectParameters;
	Entry.Draw = InItem.DrawParameters;
	Entry.World = InItem.State.World;
	Entry.bClipSpace = InItem.State.bClipSpace;
	Entry.ObjectInputs = InItem.State.ObjectInputs;
	Entry.Preparation = InItem.Preparation;
	Entry.Resolved = InItem.ResolvedParameters;
	ClassifyMaterialProviders(Entry);
	FResolvedMaterialParameters Values = *InItem.ResolvedParameters;
	Values.LocalDependenciesMask = 0;
	for (const auto Index : InCompiled->GetPass(InView.Usage).ActiveParameters)
	{
		if (std::find(Entry.SharedProviderParameters.begin(), Entry.SharedProviderParameters.end(), Index) ==
		    Entry.SharedProviderParameters.end())
		{
			Values.LocalDependenciesMask |= Values.Dependencies[Index];
		}
	}
	Entry.Resolved = InItem.ResolvedParameters;
	if (!Entry.SharedProviderParameters.empty())
	{
		const auto& Shared = InProviders.PrepareShared(Entry, InCompiled->GetPass(InView.Usage), InInputs);
		Entry.SharedGroup = Shared.Group;
		Values.Values.SetShared(Shared.Values);
		Values.Dependencies.SetShared(Shared.Dependencies);
	}
	Entry.Inputs = InProviders.RetainInputs(InInputs, Entry.Dependencies);
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((Entry.Dependencies & (1U << Scope)) == 0)
		{
			Values.Scopes.Set(Scope, {});
		}
	}
	Values.Scopes.ShareEngine({Entry.Inputs, &Entry.Inputs->Scopes});
	Entry.Resolved = std::make_shared<const FResolvedMaterialParameters>(std::move(Values));
	PrepareSharedMaterialEligibility(Entry);
	Entry.SharedBinding = InProviders.RetainSharedBinding(Entry);
	InItem.ResolvedParameters = Entry.Resolved;
	InItem.SharedParameters.reset();
	InItem.SharedBinding = Entry.SharedBinding;
	Entries.insert_or_assign(Key, std::move(Entry));
}

} // namespace

void FillMaterialObjectInputs(FMaterialProviderInputs& InInputs, const FRenderItem& InItem,
                              const FRenderSceneSnapshot& InSnapshot, const FRenderResourceService& InResources)
{
	const auto WorldViewProjection =
	    InItem.State.bClipSpace ? InItem.State.World : Multiply(InSnapshot.View.ViewProjection, InItem.State.World);
	FMaterialParameterValues ObjectValues = {
	    {"Engine.Object.World", FMaterialValue::Matrix(InItem.State.World)},
	    {"Engine.Object.Normal", FMaterialValue::Matrix(NormalMatrix(InItem.State.World))},
	    {"Engine.Object.OrientationSign", FMaterialValue::Float(Determinant(InItem.State.World) < 0 ? -1.f : 1.f)},
	    {"Engine.Object.WorldViewProjection", FMaterialValue::Matrix(WorldViewProjection)}};
	ObjectValues.insert(ObjectValues.end(), InItem.State.ObjectInputs.begin(), InItem.State.ObjectInputs.end());
	InInputs.Values[ScopeIndex(EMaterialScope::Object)] = std::move(ObjectValues);
	const auto Key = ItemKey(InItem, InSnapshot);
	const auto Lifetime = ObjectLifetime(InItem, InSnapshot, Key, InResources);
	InInputs.Scopes[ScopeIndex(EMaterialScope::Object)] = {Key, Lifetime};
}

void FillMaterialDrawInputs(FMaterialProviderInputs& InInputs, const FRenderItem& InItem,
                            const FRenderSceneSnapshot& InSnapshot)
{
	InInputs.Scopes[ScopeIndex(EMaterialScope::Draw)] = {
	    {InSnapshot.Frame->Session,
	     InSnapshot.Frame->Frame,
	     {InSnapshot.Family, InSnapshot.View.Identity, InItem.Primitive.Scene, InItem.Primitive.Slot,
	      InItem.Primitive.Generation, InItem.LocalItemId.has_value(), InItem.LocalItemId.value_or(0), InItem.Ordinal}},
	    InSnapshot.Frame->Inputs.Scopes[ScopeIndex(EMaterialScope::Frame)].Lifetime};
	InInputs.Values[ScopeIndex(EMaterialScope::Draw)] = InItem.DrawInputs;
}

FMaterialProviderInputs FRenderSession::PrepareViewInputs(const FRenderSceneSnapshot& InSnapshot)
{
	auto Inputs = InSnapshot.Frame->Inputs;
	auto& View = MaterialState->Views[InSnapshot.View.Identity];
	View.AccessFrame = InSnapshot.Frame->Frame;
	FMaterialParameterValues ViewValues{
	    {"Engine.View.ViewProjection", FMaterialValue::Matrix(InSnapshot.View.ViewProjection)},
	    {"Engine.View.CameraPosition", FMaterialValue::Float(InSnapshot.View.Eye)}};
	ViewValues.insert(ViewValues.end(), InSnapshot.View.Parameters.begin(), InSnapshot.View.Parameters.end());
	FMaterialInputValues PublishedView(std::move(ViewValues));
	if (!View.Scope.Lifetime || View.Values != PublishedView)
	{
		View.Scope.Lifetime = Resources.CreateScopeLifetime();
		View.Scope.Key.Identity = MaterialState->Identity;
		++View.Scope.Key.Revision;
		View.Scope.Key.SetQualifiers({InSnapshot.View.Identity});
		View.Values = std::move(PublishedView);
	}
	Inputs.Scopes[ScopeIndex(EMaterialScope::View)] = View.Scope;
	Inputs.Values[ScopeIndex(EMaterialScope::View)] = View.Values;
	Inputs.Scopes[ScopeIndex(EMaterialScope::Pass)] = {
	    {MaterialState->Identity, InSnapshot.Frame->Frame, {InSnapshot.Family, InSnapshot.View.Identity}},
	    Resources.CreateScopeLifetime()};
	Inputs.Values[ScopeIndex(EMaterialScope::Pass)] = InSnapshot.View.PassParameters;
	return Inputs;
}

namespace
{
void PrepareMaterialItem(FRenderItem& InItem, FRenderSceneSnapshot& InSnapshot, FMaterialProviderInputs& InInputs,
                         FViewMaterialProviders& InProviders, const FRenderResourceService& InResources,
                         FMaterialPreparationProfile& InProfile, bool bInRetainLocal)
{
	if (!InItem.State.Surface)
	{
		return;
	}
	try
	{
		if (bInRetainLocal && InItem.SharedBinding && InItem.ResolvedParameters &&
		    InItem.SharedBinding->Pass->Usage == InSnapshot.View.Usage)
		{
			const auto& Update = InProviders.UpdateSharedBinding(InItem.SharedBinding, InInputs);
			if (Update)
			{
				InItem.SharedParameters = Update;
				InProfile.SharedUpdate();
				++InSnapshot.Statistics.SharedMaterialUpdates;
				++InSnapshot.Statistics.RetainedMaterialItems;
				return;
			}
		}
		const auto Compiled = InItem.Preparation && InItem.Preparation->Program ? InItem.Preparation->Program
		                                                                        : InItem.State.Surface->GetCompiled();
		if (((!InItem.Preparation || !InItem.Preparation->Program) &&
		     InItem.State.Surface->GetStatus() != ERenderMaterialStatus::Ready) ||
		    !Compiled)
		{
			throw std::runtime_error("Material resources are not ready: " + InItem.State.Surface->GetError());
		}
		if (!InItem.Preparation || !InItem.Preparation->Program)
		{
			InItem.Context.ObjectParameters = GetPrimitiveMaterialOverrides(InItem.State, *Compiled->Interface.Schema);
		}
		if (ReuseEvaluation(InItem, InSnapshot.View, InInputs, Compiled))
		{
			InProfile.Reused();
			return;
		}
		if (ShareMaterialEvaluation(InItem, InSnapshot, InInputs, Compiled, InProviders))
		{
			InProfile.SharedUpdate();
			++InSnapshot.Statistics.SharedMaterialUpdates;
			return;
		}
		if (RefreshMaterialEvaluation(InItem, InSnapshot, InInputs, InResources, Compiled, InProviders))
		{
			InProfile.Refreshed();
			return;
		}
		InProfile.Evaluated();
		HYP_PERF_SCOPE_C(Detail, FullMaterialEvaluation);
		const auto& Pass = Compiled->GetPass(InSnapshot.View.Usage);
		FillMaterialObjectInputs(InInputs, InItem, InSnapshot, InResources);
		FillMaterialDrawInputs(InInputs, InItem, InSnapshot);
		std::vector<std::string> Semantics;
		for (const auto Index : Pass.ActiveParameters)
		{
			const auto& Parameter = Compiled->Interface.Schema->GetParameters()[Index];
			if (Parameter.Source == EMaterialParameterSource::Semantic)
			{
				Semantics.push_back(Parameter.Semantic);
			}
		}
		InItem.Context.Scopes = InInputs.Scopes;
		InItem.Context.Providers = InProviders.Registry.Evaluate(InInputs, Semantics);
		InItem.Context.ObjectParameters = GetPrimitiveMaterialOverrides(InItem.State, *Compiled->Interface.Schema);
		InItem.Context.DrawParameters = InItem.DrawParameters;
		// Validate the complete logical draw before dispatching any native work for the family.
		InItem.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(
		    ResolveMaterialBindingContext(InItem.State.Surface->GetSnapshot(), *Compiled, Pass, InItem.Context));
		CacheEvaluation(InItem, InSnapshot.View, InInputs, Compiled, InProviders);
	}
	catch (const std::exception& Error)
	{
		InItem.PreparationError = Error.what();
	}
}
} // namespace

void FRenderSession::PrepareMaterials(FRenderSceneSnapshot& InSnapshot, bool bInStableCollection)
{
	HYP_PERF_SCOPE_C(Material, PrepareMaterials);
	FMaterialPreparationProfile Profile(MaterialState->Providers);
	auto Inputs = PrepareViewInputs(InSnapshot);
	FViewMaterialProviders ViewProviders{MaterialState->Providers};
	ViewProviders.BindingHistory = &MaterialState->SharedBindings;
	const bool bRetainLocal = bInStableCollection && !Batches.HasCustomStrategies();
	const bool bGrouped = bRetainLocal && UpdateMaterialBindingGroups(InSnapshot, ViewProviders, Inputs);
	bool bRegroup = !bGrouped;
	if (bGrouped)
	{
		const auto& Groups = *InSnapshot.SharedBindingGroups;
		Profile.SharedUpdate(Groups.ItemCount - Groups.Ungrouped.size());
		for (const auto Index : Groups.Ungrouped)
		{
			auto& Item = InSnapshot.Items[Index];
			PrepareMaterialItem(Item, InSnapshot, Inputs, ViewProviders, Resources, Profile, bRetainLocal);
			bRegroup |= bool(Item.SharedBinding);
		}
	}
	else
	{
		for (auto& Item : InSnapshot.Items)
		{
			PrepareMaterialItem(Item, InSnapshot, Inputs, ViewProviders, Resources, Profile, bRetainLocal);
		}
	}
	if (bRegroup)
	{
		// Build an index only after a view demonstrates stable membership. Churning views still use the
		// per-item shared-update path, without paying to construct a group index that expires next frame.
		const bool bStableMembers = InSnapshot.Statistics.CollectionReuses || InSnapshot.Statistics.MembershipReuses;
		InSnapshot.SharedBindingGroups =
		    bRetainLocal && bStableMembers ? RetainMaterialBindingGroups(InSnapshot) : nullptr;
	}
	InSnapshot.Statistics.SharedMaterialGroups = ViewProviders.BindingEvaluations;
	HYP_PERF_PLOT(Material, SharedMaterialGroupUpdates, double(ViewProviders.BindingEvaluations));
}
} // namespace Hyperion
