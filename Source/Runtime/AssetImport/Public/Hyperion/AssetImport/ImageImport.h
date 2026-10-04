#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"

namespace Hyperion
{
inline constexpr char ImageImporterId[] = "hyperion.image";

void RegisterImageImporter(FAssetImportService& InImports);
} // namespace Hyperion
