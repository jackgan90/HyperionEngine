#include "Hyperion/Renderer/RenderResources.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
FMaterialParameterValues GetPrimitiveMaterialOverrides(const FRenderPrimitiveState& InState,
                                                       const FMaterialParameterSchema& InSchema)
{
	FMaterialParameterValues Result;
	const auto AddLegacy = [&](std::string_view InSemantic, FMaterialValue InValue)
	{
		const auto& Parameters = InSchema.GetParameters();
		const auto It = std::find_if(Parameters.begin(), Parameters.end(),
		                             [&](const auto& InParameter)
		                             {
			                             return InParameter.Semantic == InSemantic;
		                             });
		if (It != Parameters.end())
		{
			Result.push_back({It->Name, std::move(InValue)});
		}
	};
	if (InState.Material.BaseColor)
	{
		AddLegacy("Pbr.BaseColorFactor", FMaterialValue::Float(*InState.Material.BaseColor));
	}
	if (InState.Material.Metallic)
	{
		AddLegacy("Pbr.MetallicFactor", FMaterialValue::Float(*InState.Material.Metallic));
	}
	if (InState.Material.Roughness)
	{
		AddLegacy("Pbr.RoughnessFactor", FMaterialValue::Float(*InState.Material.Roughness));
	}
	for (const auto* Level : {&InState.ObjectParameters, &InState.SectionParameters})
	{
		std::set<std::size_t> Seen;
		for (const auto& Override : *Level)
		{
			const auto Handle = InSchema.Find(Override.Name);
			if (!Seen.insert(Handle.Index).second)
			{
				throw std::invalid_argument("Duplicate aliases within a primitive override level");
			}
			std::erase_if(Result,
			              [&](const auto& InValue)
			              {
				              return InSchema.Find(InValue.Name).Index == Handle.Index;
			              });
			Result.push_back({InSchema.Get(Handle).Name, Override.Value});
		}
	}
	return Result;
}
} // namespace Hyperion
