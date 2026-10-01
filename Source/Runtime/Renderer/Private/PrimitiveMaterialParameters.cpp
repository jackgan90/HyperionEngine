#include "Hyperion/Materials/PbrParameters.h"
#include "Hyperion/Renderer/RenderResources.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
FMaterialParameterValues GetPrimitiveMaterialOverrides(const FRenderPrimitiveState& InState,
                                                       const FMaterialParameterSchema& InSchema)
{
	FMaterialParameterValues Result;
	const auto AddLegacy = [&](FMaterialSemanticId InSemantic, FMaterialValue InValue)
	{
		const auto& Parameters = InSchema.GetParameters();
		const auto It = std::find_if(Parameters.begin(), Parameters.end(),
		                             [&](const auto& InParameter)
		                             {
			                             return InParameter.Semantic == InSemantic;
		                             });
		if (It != Parameters.end())
		{
			Result.push_back(
			    {InSchema.GetHandle(static_cast<std::size_t>(It - Parameters.begin())), std::move(InValue)});
		}
	};
	if (InState.Material.BaseColor)
	{
		AddLegacy(EHyperionMaterialV1Field::BaseColor, FMaterialValue::Float(*InState.Material.BaseColor));
	}
	if (InState.Material.Metallic)
	{
		AddLegacy(EHyperionMaterialV1Field::Metallic, FMaterialValue::Float(*InState.Material.Metallic));
	}
	if (InState.Material.Roughness)
	{
		AddLegacy(EHyperionMaterialV1Field::Roughness, FMaterialValue::Float(*InState.Material.Roughness));
	}
	for (const auto* Level : {&InState.ObjectParameters, &InState.SectionParameters})
	{
		std::set<std::size_t> Seen;
		for (const auto& Override : *Level)
		{
			const auto Handle = Override.Resolve(InSchema);
			if (!Seen.insert(Handle.Index).second)
			{
				throw std::invalid_argument("Duplicate aliases within a primitive override level");
			}
			std::erase_if(Result,
			              [&](const auto& InValue)
			              {
				              return InValue.Handle.Index == Handle.Index;
			              });
			Result.push_back({Handle, Override.Value});
		}
	}
	return Result;
}
} // namespace Hyperion
