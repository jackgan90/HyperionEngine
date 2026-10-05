#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"

namespace Hyperion
{
FAssetImporter NormalizeAssetImporter(FAssetImporter InImporter);
const FAssetImporter* FindAssetImporter(std::span<const FAssetImporter> InImporters, std::string_view InExtension,
                                        std::string_view InType = {}, bool bInWorkspaceInference = false);
std::vector<FAssetImporter> DefaultAssetImporters();
} // namespace Hyperion
