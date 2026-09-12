#pragma once
#include "Hyperion/AssetImport/ModelImport.h"
#include "Hyperion/Renderer/Model.h"
#include <mutex>

namespace Hyperion
{
// Procedural rendering fixtures author the source DTO, then exercise the same independent CPU assets as import.
inline std::shared_ptr<const FSceneModelData> PrepareSourceModel(std::shared_ptr<const FModelSource> InSource)
{
	static std::mutex Mutex;
	using FEntry = std::pair<std::weak_ptr<const FModelSource>, std::weak_ptr<const FSceneModelData>>;
	static std::map<const FModelSource*, FEntry> Cache;
	std::lock_guard Lock(Mutex);
	std::erase_if(Cache,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.first.expired() || InEntry.second.second.expired();
	              });
	if (const auto It = Cache.find(InSource.get()); It != Cache.end())
	{
		if (auto Existing = It->second.second.lock())
		{
			return Existing;
		}
	}
	auto Split = SplitModelSource(*InSource);
	std::map<std::string, FAssetImportProduct> Products;
	for (const auto& Product : Split.Products)
	{
		Products.emplace("@" + Product.Key, Product);
	}
	std::vector<std::shared_ptr<const FMaterialAssetData>> Materials;
	for (const auto& Reference : Split.Model.MaterialSlots)
	{
		auto Data = std::make_shared<FMaterialAssetData>();
		Data->Asset = std::static_pointer_cast<const FMaterialAsset>(Products.at(Reference.Path).Object);
		for (const auto& Dependency : CollectAssetDependencies(RecordType<FMaterialAsset>(), Data->Asset.get()))
		{
			Data->Textures.emplace(Dependency.Reference, std::static_pointer_cast<const FTextureAsset>(
			                                                 Products.at(Dependency.Reference.Path).Object));
		}
		Materials.push_back(std::move(Data));
	}
	auto Data = PrepareSceneModel(std::make_shared<const FModelAsset>(std::move(Split.Model)), std::move(Materials));
	Cache[InSource.get()] = {InSource, Data};
	return Data;
}

class FSourceModel : public FModel
{
public:
	using FModel::FModel;

	FSourceModel(FRenderSceneClient& InScene, FRenderResourceService& InResources,
	             std::shared_ptr<const FModelSource> InSource)
	    : FModel(InScene, InResources, FSceneModel{"", PrepareSourceModel(std::move(InSource))})
	{
	}
};
} // namespace Hyperion
