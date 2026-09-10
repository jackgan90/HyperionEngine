#pragma once
#include "MaterialEvaluationCache.h"

namespace Hyperion
{
struct FMaterialBindingGroups
{
	struct FGroup
	{
		std::shared_ptr<const FMaterialSharedBinding> Binding;
		std::vector<std::size_t> Items;
	};

	std::vector<FGroup> Groups;
	std::vector<std::size_t> Ungrouped;
	std::size_t ItemCount{};
};

std::shared_ptr<const FMaterialBindingGroups> RetainMaterialBindingGroups(const FRenderSceneSnapshot& InSnapshot);
bool UpdateMaterialBindingGroups(FRenderSceneSnapshot& InSnapshot, FViewMaterialProviders& InProviders,
                                 const FMaterialProviderInputs& InInputs);
} // namespace Hyperion
