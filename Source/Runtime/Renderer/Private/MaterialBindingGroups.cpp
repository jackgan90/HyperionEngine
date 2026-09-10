#include "MaterialBindingGroups.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderPrimitive.h"
#include "MaterialSharedBinding.h"
#include <algorithm>
#include <exception>

namespace Hyperion
{
std::shared_ptr<const FMaterialBindingGroups> RetainMaterialBindingGroups(const FRenderSceneSnapshot& InSnapshot)
{
	HYP_PERF_SCOPE_C(Material, RetainMaterialGroups);
	if (InSnapshot.Items.Size() > 4096)
	{
		return {};
	}
	auto Result = std::make_shared<FMaterialBindingGroups>();
	Result->ItemCount = InSnapshot.Items.Size();
	for (std::size_t Index = 0; Index < Result->ItemCount; ++Index)
	{
		const auto& Item = InSnapshot.Items[Index];
		if (!Item.SharedBinding || !Item.ResolvedParameters || !Item.Preparation || !Item.PreparationError.empty() ||
		    Item.SharedBinding->Pass->Usage != InSnapshot.View.Usage)
		{
			Result->Ungrouped.push_back(Index);
			continue;
		}
		const auto It = std::find_if(Result->Groups.begin(), Result->Groups.end(),
		                             [&](const auto& InGroup)
		                             {
			                             return InGroup.Binding == Item.SharedBinding;
		                             });
		if (It != Result->Groups.end())
		{
			It->Items.push_back(Index);
		}
		else
		{
			if (Result->Groups.size() >= 128)
			{
				Result->Ungrouped.push_back(Index);
				continue;
			}
			Result->Groups.push_back({Item.SharedBinding, {Index}});
		}
	}
	return Result->Groups.empty() ? nullptr : Result;
}

bool UpdateMaterialBindingGroups(FRenderSceneSnapshot& InSnapshot, FViewMaterialProviders& InProviders,
                                 const FMaterialProviderInputs& InInputs)
{
	HYP_PERF_SCOPE_C(Material, UpdateRetainedMaterialGroups);
	const auto& Groups = InSnapshot.SharedBindingGroups;
	if (!Groups || Groups->ItemCount != InSnapshot.Items.Size())
	{
		return false;
	}
	std::vector<std::shared_ptr<const FMaterialSharedParameters>> Updates;
	Updates.reserve(Groups->Groups.size());
	try
	{
		for (const auto& Group : Groups->Groups)
		{
			if (Group.Binding->Pass->Usage != InSnapshot.View.Usage)
			{
				return false;
			}
			const auto& Update = InProviders.UpdateSharedBinding(Group.Binding, InInputs);
			if (!Update)
			{
				return false;
			}
			Updates.push_back(Update);
		}
	}
	catch (const std::exception&)
	{
		return false; // The complete path publishes failures with the original per-source transaction.
	}

	// Publish only after every group has validated current availability, dependencies and resources.
	for (std::size_t Index = 0; Index < Groups->Groups.size(); ++Index)
	{
		for (const auto Item : Groups->Groups[Index].Items)
		{
			InSnapshot.Items[Item].SharedParameters = Updates[Index];
		}
	}
	InSnapshot.Statistics.SharedMaterialUpdates = Groups->ItemCount - Groups->Ungrouped.size();
	InSnapshot.Statistics.RetainedMaterialItems = Groups->ItemCount - Groups->Ungrouped.size();
	InSnapshot.Statistics.SharedMaterialGroups = InProviders.BindingEvaluations;
	return true;
}
} // namespace Hyperion
