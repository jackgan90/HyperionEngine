#include "Hyperion/Core/Profiling.h"
#include "MaterialEvaluationCache.h"
#include "MaterialProfiling.h"
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
	if (Entry.Snapshot != InItem.State.Surface->GetSnapshot() || Entry.Compiled != InCompiled ||
	    Entry.Usage != InView.Usage ||
	    !Entry.Matches(InInputs, InItem.Context.ObjectParameters, InItem.DrawParameters, InItem.State.World,
	                   InItem.State.bClipSpace, InItem.State.ObjectInputs))
	{
		return false;
	}
	InItem.ResolvedParameters = Entry.Resolved;
	Entry.AccessFrame = InInputs.Scopes[ScopeIndex(EMaterialScope::Frame)].Key.Revision;
	InItem.EvaluationCache->TouchObject(*InItem.LocalItemId, Entry.AccessFrame);
	return true;
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
	std::vector<bool> Overridden(InCompiled->Interface.Schema->GetParameters().size());
	const std::array<const FMaterialParameterValues*, 3> OverrideSets{&Entry.Snapshot->Overrides, &Entry.Object,
	                                                                  &Entry.Draw};
	for (const auto* Overrides : OverrideSets)
	{
		for (const auto& Override : *Overrides)
		{
			Overridden[InCompiled->Interface.Schema->Find(Override.Name).Index] = true;
		}
	}
	for (const auto Index : InCompiled->GetPass(InView.Usage).ActiveParameters)
	{
		Entry.Dependencies |= InItem.ResolvedParameters->Dependencies[Index];
		if (!Overridden[Index] &&
		    InCompiled->Interface.Schema->GetParameters()[Index].Source == EMaterialParameterSource::Semantic)
		{
			constexpr auto PerItem = MaterialScopeBit(EMaterialScope::Object) |
			                         MaterialScopeBit(EMaterialScope::Material) |
			                         MaterialScopeBit(EMaterialScope::Draw);
			if ((InItem.ResolvedParameters->Dependencies[Index] & PerItem) == 0)
			{
				Entry.SharedProviderParameters.push_back(Index);
			}
			else
			{
				Entry.ProviderParameters.push_back(Index);
			}
		}
	}
	FResolvedMaterialParameters Values = *InItem.ResolvedParameters;
	Entry.Resolved = InItem.ResolvedParameters;
	if (!Entry.SharedProviderParameters.empty())
	{
		const auto Shared = InProviders.PrepareShared(Entry, InCompiled->GetPass(InView.Usage), InInputs);
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
	InItem.ResolvedParameters = Entry.Resolved;
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

void FRenderSession::PrepareMaterials(FRenderSceneSnapshot& InSnapshot)
{
	HYP_PERF_SCOPE_C(Material, PrepareMaterials);
	FMaterialPreparationProfile Profile(MaterialState->Providers);
	auto Inputs = InSnapshot.Frame->Inputs;
	auto& View = MaterialState->Views[InSnapshot.View.Identity];
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
	FViewMaterialProviders ViewProviders{MaterialState->Providers};
	for (auto& Item : InSnapshot.Items)
	{
		if (!Item.State.Surface)
		{
			continue;
		}
		try
		{
			const auto Compiled = Item.State.Surface->GetCompiled();
			if (Item.State.Surface->GetStatus() != ERenderMaterialStatus::Ready || !Compiled)
			{
				throw std::runtime_error("Material resources are not ready: " + Item.State.Surface->GetError());
			}
			const auto& Pass = Compiled->GetPass(InSnapshot.View.Usage);
			Item.Context.ObjectParameters = GetPrimitiveMaterialOverrides(Item.State, *Compiled->Interface.Schema);
			if (ReuseEvaluation(Item, InSnapshot.View, Inputs, Compiled))
			{
				Profile.Reused();
				continue;
			}
			if (RefreshMaterialEvaluation(Item, InSnapshot, Inputs, Resources, Compiled, ViewProviders))
			{
				Profile.Refreshed();
				continue;
			}
			Profile.Evaluated();
			HYP_PERF_SCOPE_C(Detail, FullMaterialEvaluation);
			FillMaterialObjectInputs(Inputs, Item, InSnapshot, Resources);
			FillMaterialDrawInputs(Inputs, Item, InSnapshot);
			std::vector<std::string> Semantics;
			for (const auto Index : Pass.ActiveParameters)
			{
				const auto& Parameter = Compiled->Interface.Schema->GetParameters()[Index];
				if (Parameter.Source == EMaterialParameterSource::Semantic)
				{
					Semantics.push_back(Parameter.Semantic);
				}
			}
			Item.Context.Scopes = Inputs.Scopes;
			Item.Context.Providers = MaterialState->Providers.Evaluate(Inputs, Semantics);
			Item.Context.ObjectParameters = GetPrimitiveMaterialOverrides(Item.State, *Compiled->Interface.Schema);
			Item.Context.DrawParameters = Item.DrawParameters;
			// Validate the complete logical draw before dispatching any native work for the family.
			Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(
			    ResolveMaterialBindingContext(Item.State.Surface->GetSnapshot(), *Compiled, Pass, Item.Context));
			CacheEvaluation(Item, InSnapshot.View, Inputs, Compiled, ViewProviders);
		}
		catch (const std::exception& Error)
		{
			Item.PreparationError = Error.what();
		}
	}
}
} // namespace Hyperion
