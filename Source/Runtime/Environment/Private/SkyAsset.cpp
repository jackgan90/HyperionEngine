#include "Hyperion/Environment/SkyAsset.h"
#include <cmath>

namespace Hyperion
{
void ValidateSkyAsset(const FSkyAsset& InAsset)
{
	if (InAsset.Convention != 1)
	{
		throw std::invalid_argument("Unsupported environment bake convention");
	}
	for (const auto* Reference : {&InAsset.Radiance, &InAsset.Specular, &InAsset.Brdf})
	{
		ValidateAssetRef(*Reference);
		if (Reference->TypeId != RecordType<FTextureAsset>().Id || (Reference->Path.empty() && Reference->Id.empty()))
		{
			throw std::invalid_argument("Sky requires typed native texture dependencies");
		}
	}
	for (const auto& Coefficient : InAsset.Irradiance)
	{
		for (const float Value : Coefficient)
		{
			if (!std::isfinite(Value))
			{
				throw std::invalid_argument("Sky requires finite irradiance coefficients");
			}
		}
	}
}

template<> const FRecordDescriptor& RecordType<FSkyAsset>()
{
	static const auto Type = MakeRecord<FSkyAsset>(
	    "hyperion.skyasset",
	    {Member("name", &FSkyAsset::Name), Member("radiance", &FSkyAsset::Radiance),
	     Member("specular", &FSkyAsset::Specular), Member("brdf", &FSkyAsset::Brdf),
	     Member("irradiance", &FSkyAsset::Irradiance), Member("convention", &FSkyAsset::Convention)},
	    1, ValidateSkyAsset);
	return Type;
}
} // namespace Hyperion
