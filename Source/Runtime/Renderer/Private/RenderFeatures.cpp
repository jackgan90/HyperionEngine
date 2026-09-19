#include "Hyperion/Renderer/RenderFeatures.h"
#include <algorithm>

namespace Hyperion
{
void FRenderFeatureRegistry::Add(std::string InId, FRenderFeatureFactory InFactory)
{
	if (bSealed || InId.empty() || !InFactory ||
	    std::any_of(Factories.begin(), Factories.end(),
	                [&](const auto& InEntry)
	                {
		                return InEntry.first == InId;
	                }))
	{
		throw std::logic_error("Invalid or late render feature registration: " + InId);
	}
	Factories.emplace_back(std::move(InId), std::move(InFactory));
}

void FRenderFeatureRegistry::Remove(const std::string& InId) noexcept
{
	std::erase_if(Factories,
	              [&](const auto& InEntry)
	              {
		              return InEntry.first == InId;
	              });
}

FRenderFeatureList FRenderFeatureRegistry::Create()
{
	bSealed = true;
	FRenderFeatureList Result;
	for (const auto& [Id, Factory] : Factories)
	{
		auto Feature = Factory();
		if (!Feature)
		{
			throw std::logic_error("Null render feature: " + Id);
		}
		Result.push_back(std::move(Feature));
	}
	return Result;
}

FRenderFeatureList MakeDefaultRenderFeatures()
{
	FRenderFeatureList Result;
	Result.push_back(MakeContactShadowFeature());
	return Result;
}
} // namespace Hyperion
