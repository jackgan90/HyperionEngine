#include "LocalMaterialPreparation.h"
#include "Hyperion/Core/Profiling.h"
#include "MaterialSharedBinding.h"
#include "SceneItemPreparation.h"

namespace Hyperion
{
std::shared_ptr<const FLocalMaterialPreparation> PrepareLocalMaterials(
    FRenderSceneSnapshot& InSnapshot, const std::shared_ptr<const FLocalMaterialPreparation>& InPrevious,
    const FRenderResourceService& InResources, bool bInStableCollection)
{
	HYP_PERF_SCOPE_C(Detail, PrepareLocalMaterialProof);
	if (InSnapshot.Items.Size() > 4096)
	{
		for (auto& Item : InSnapshot.Items)
		{
			Item.LocalPreparation.reset();
		}
		return {};
	}
	bool bMatches = InPrevious && InPrevious->Items.size() == InSnapshot.Items.Size();
	bool bEligible = true;
	constexpr auto Engine = FMaterialResolvedScopes::EngineMask | MaterialScopeBit(EMaterialScope::Draw);
	for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
	{
		auto& Item = InSnapshot.Items[Index];
		if (!Item.Preparation || !Item.ResolvedParameters || !Item.PreparationError.empty() ||
		    ((Item.ResolvedParameters->DependenciesMask & Engine) &&
		     (!Item.SharedBinding || Item.SharedBinding->Program != Item.Preparation->Program ||
		      Item.SharedBinding->Pass->Usage != InSnapshot.View.Usage)))
		{
			Item.LocalPreparation.reset();
			bEligible = false;
			continue;
		}
		if (!bInStableCollection || !Item.LocalPreparation ||
		    Item.LocalPreparation->Preparation.lock() != Item.Preparation ||
		    Item.LocalPreparation->Parameters.lock() != Item.ResolvedParameters)
		{
			Item.LocalPreparation = std::make_shared<const FLocalMaterialItem>(
			    FLocalMaterialItem{Item.Preparation, Item.ResolvedParameters});
		}
		if (bMatches)
		{
			const auto& Previous = InPrevious->Items[Index];
			bMatches = Previous.Primitive == Item.Primitive && Previous.Ordinal == Item.Ordinal &&
			           Previous.LocalId == Item.LocalItemId && Previous.Preparation.lock() == Item.Preparation &&
			           Previous.Parameters.lock() == Item.ResolvedParameters;
		}
	}
	if (!bEligible)
	{
		return {};
	}
	if (bMatches)
	{
		return InPrevious;
	}
	auto Result = std::make_shared<FLocalMaterialPreparation>();
	Result->Lifetime = InResources.CreateScopeLifetime();
	Result->Items.reserve(InSnapshot.Items.Size());
	for (const auto& Item : InSnapshot.Items)
	{
		Result->Items.push_back(
		    {Item.Primitive, Item.Ordinal, Item.LocalItemId, Item.Preparation, Item.ResolvedParameters});
	}
	return Result;
}
} // namespace Hyperion
