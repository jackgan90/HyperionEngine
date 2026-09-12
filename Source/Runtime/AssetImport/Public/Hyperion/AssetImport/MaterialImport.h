#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"

namespace Hyperion
{
void RegisterMaterialImporters(FAssetImportService& InImports);
// Editable, tagged JSON representation of the same generic reflected record tree.
FArchiveNode DecodeAssetSourceJson(std::string_view InText);
std::string EncodeAssetSourceJson(const FArchiveNode& InNode);
} // namespace Hyperion
