#include "Hyperion/Scene/SceneManifest.h"
#include <set>

namespace Hyperion
{
void ValidateSceneMaterialAsset(const FSceneMaterialAsset& InMaterial)
{
	if (InMaterial.Reference)
	{
		ValidateAssetRef(*InMaterial.Reference);
		if (InMaterial.Reference->TypeId != RecordType<FMaterialAsset>().Id)
		{
			throw std::invalid_argument("Scene material selection requires a material asset reference");
		}
	}
	else if (!InMaterial.Values.empty())
	{
		throw std::invalid_argument("Material instance values require an explicit selected asset");
	}
	for (const auto* Values : {&InMaterial.Values, &InMaterial.Overrides})
	{
		std::set<std::string> Names;
		for (const auto& Entry : *Values)
		{
			if (Entry.Name.empty() || !Names.insert(Entry.Name).second)
			{
				throw std::invalid_argument("Duplicate or empty persistent scene material value");
			}
			ValidateMaterialAssetValue(Entry.Value);
		}
	}
}

template<> const FRecordDescriptor& RecordType<FSceneMaterialAsset>()
{
	static const auto Type = MakeRecord<FSceneMaterialAsset>("hyperion.scenematerialasset",
	                                                         {Member("reference", &FSceneMaterialAsset::Reference),
	                                                          Member("values", &FSceneMaterialAsset::Values),
	                                                          Member("overrides", &FSceneMaterialAsset::Overrides)},
	                                                         1, ValidateSceneMaterialAsset);
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneSectionMaterial>()
{
	static const auto Type = MakeRecord<FSceneSectionMaterial>(
	    "hyperion.scenesectionmaterial",
	    {Member("section", &FSceneSectionMaterial::Section), Member("material", &FSceneSectionMaterial::Material)});
	return Type;
}
} // namespace Hyperion
