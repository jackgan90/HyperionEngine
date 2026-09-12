#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/ModelSource.h"

namespace Hyperion
{
struct FModelSourceAssets
{
	FModelAsset Model;
	std::vector<FAssetImportProduct> Products;
};

// Produces immutable named material/texture records. References beginning with @ denote named products.
FModelSourceAssets SplitModelSource(const FModelSource& InSource);
std::shared_ptr<FModelAsset> EmitModelSource(FAssetImportContext& InContext, const FModelSource& InSource);
FModelSource ReadEmbeddedModelSource(const FAssetDocument& InDocument);
} // namespace Hyperion
