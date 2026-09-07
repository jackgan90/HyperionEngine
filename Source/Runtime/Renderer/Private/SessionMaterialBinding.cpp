#include "MaterialEvaluationCache.h"
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
	FMaterialScopeKey Key{
	    InItem.Primitive.Scene,
	    1,
	    {InItem.Primitive.Slot, InItem.Primitive.Generation, InItem.LocalItemId.value_or(InItem.Ordinal)}};
	if (!InItem.LocalItemId)
	{
		Key.Qualifiers.insert(Key.Qualifiers.end(),
		                      {InSnapshot.Frame->Frame, InSnapshot.Family, InSnapshot.View.Identity, InItem.Ordinal});
	}
	for (const auto Value : InItem.State.World.Values)
	{
		Key.Qualifiers.push_back(std::bit_cast<std::uint32_t>(Value));
	}
	Key.Qualifiers.push_back(InItem.State.bClipSpace);
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
		return FrameLifetime;
	}
	auto& Entry = Objects[*InItem.LocalItemId];
	if (!Entry.Scope.Lifetime || Entry.Scope.Key != InKey || Entry.Inputs != InItem.State.ObjectInputs ||
	    Entry.Overrides != InItem.Context.ObjectParameters)
	{
		// Retire previous emitted contents even when the primitive's publication revision is unchanged.
		Entry = {
		    {InKey, InResources.CreateScopeLifetime()}, InItem.State.ObjectInputs, InItem.Context.ObjectParameters};
	}
	return Entry.Scope.Lifetime;
}

void FillObjectInputs(FMaterialProviderInputs& InInputs, const FRenderItem& InItem,
                      const FRenderSceneSnapshot& InSnapshot, const FRenderResourceService& InResources)
{
	const auto WorldViewProjection =
	    InItem.State.bClipSpace ? InItem.State.World : Multiply(InSnapshot.View.ViewProjection, InItem.State.World);
	InInputs.Values[ScopeIndex(EMaterialScope::Object)] = {
	    {"Engine.Object.World", FMaterialValue::Matrix(InItem.State.World)},
	    {"Engine.Object.Normal", FMaterialValue::Matrix(NormalMatrix(InItem.State.World))},
	    {"Engine.Object.OrientationSign", FMaterialValue::Float(Determinant(InItem.State.World) < 0 ? -1.f : 1.f)},
	    {"Engine.Object.WorldViewProjection", FMaterialValue::Matrix(WorldViewProjection)}};
	auto& ObjectValues = InInputs.Values[ScopeIndex(EMaterialScope::Object)];
	ObjectValues.insert(ObjectValues.end(), InItem.State.ObjectInputs.begin(), InItem.State.ObjectInputs.end());
	const auto Key = ItemKey(InItem, InSnapshot);
	const auto Lifetime = ObjectLifetime(InItem, InSnapshot, Key, InResources);
	InInputs.Scopes[ScopeIndex(EMaterialScope::Object)] = {Key, Lifetime};
	auto DrawKey = Key;
	DrawKey.Qualifiers.insert(DrawKey.Qualifiers.end(),
	                          {InSnapshot.Frame->Frame, InSnapshot.Family, InSnapshot.View.Identity});
	InInputs.Scopes[ScopeIndex(EMaterialScope::Draw)] = {
	    std::move(DrawKey), InSnapshot.Frame->Inputs.Scopes[ScopeIndex(EMaterialScope::Frame)].Lifetime};
	InInputs.Values[ScopeIndex(EMaterialScope::Draw)] = InItem.DrawInputs;
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
	const auto& Entry = It->second;
	if (Entry.Snapshot != InItem.State.Surface->GetSnapshot() || Entry.Compiled != InCompiled ||
	    Entry.Usage != InView.Usage ||
	    !Entry.Matches(InInputs, InItem.Context.ObjectParameters, InItem.DrawParameters, InItem.State.World,
	                   InItem.State.bClipSpace, InItem.State.ObjectInputs))
	{
		return false;
	}
	InItem.ResolvedParameters = Entry.Resolved;
	return true;
}

void CacheEvaluation(FRenderItem& InItem, const FRenderView& InView, const FMaterialProviderInputs& InInputs,
                     const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled)
{
	if (!InItem.EvaluationCache || !InItem.LocalItemId)
	{
		return;
	}
	const auto Key = std::pair{*InItem.LocalItemId, InView.Identity};
	auto& Entries = InItem.EvaluationCache->Entries;
	if (!Entries.contains(Key) && Entries.size() >= 64)
	{
		return;
	}
	FMaterialEvaluationCache::FEntry Entry;
	Entry.Snapshot = InItem.State.Surface->GetSnapshot();
	Entry.Compiled = InCompiled;
	Entry.Usage = InView.Usage;
	Entry.Object = InItem.Context.ObjectParameters;
	Entry.Draw = InItem.DrawParameters;
	Entry.World = InItem.State.World;
	Entry.bClipSpace = InItem.State.bClipSpace;
	Entry.ObjectInputs = InItem.State.ObjectInputs;
	for (const auto Index : InCompiled->GetPass(InView.Usage).ActiveParameters)
	{
		Entry.Dependencies |= InItem.ResolvedParameters->Dependencies[Index];
	}
	FResolvedMaterialParameters Values = *InItem.ResolvedParameters;
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if (Entry.Dependencies & (1U << Scope))
		{
			Entry.Inputs.Scopes[Scope] = InInputs.Scopes[Scope];
			Entry.Inputs.Values[Scope] = InInputs.Values[Scope];
		}
		else
		{
			Values.Scopes[Scope] = {};
		}
	}
	Entry.Resolved = std::make_shared<const FResolvedMaterialParameters>(std::move(Values));
	InItem.ResolvedParameters = Entry.Resolved;
	Entries.insert_or_assign(Key, std::move(Entry));
}
} // namespace

void FRenderSession::PrepareMaterials(FRenderSceneSnapshot& InSnapshot)
{
	auto Inputs = InSnapshot.Frame->Inputs;
	auto& View = MaterialState->Views[InSnapshot.View.Identity];
	FMaterialParameterValues ViewValues{
	    {"Engine.View.ViewProjection", FMaterialValue::Matrix(InSnapshot.View.ViewProjection)},
	    {"Engine.View.CameraPosition", FMaterialValue::Float(InSnapshot.View.Eye)}};
	ViewValues.insert(ViewValues.end(), InSnapshot.View.Parameters.begin(), InSnapshot.View.Parameters.end());
	if (!View.Scope.Lifetime || View.Values != ViewValues)
	{
		View.Scope.Lifetime = Resources.CreateScopeLifetime();
		View.Scope.Key.Identity = MaterialState->Identity;
		++View.Scope.Key.Revision;
		View.Scope.Key.Qualifiers = {InSnapshot.View.Identity};
		View.Values = std::move(ViewValues);
	}
	Inputs.Scopes[ScopeIndex(EMaterialScope::View)] = View.Scope;
	Inputs.Values[ScopeIndex(EMaterialScope::View)] = View.Values;
	Inputs.Scopes[ScopeIndex(EMaterialScope::Pass)] = {
	    {MaterialState->Identity, InSnapshot.Frame->Frame, {InSnapshot.Family, InSnapshot.View.Identity}},
	    Resources.CreateScopeLifetime()};
	Inputs.Values[ScopeIndex(EMaterialScope::Pass)] = InSnapshot.View.PassParameters;
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
				continue;
			}
			FillObjectInputs(Inputs, Item, InSnapshot, Resources);
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
			CacheEvaluation(Item, InSnapshot.View, Inputs, Compiled);
		}
		catch (const std::exception& Error)
		{
			Item.PreparationError = Error.what();
		}
	}
}
} // namespace Hyperion
