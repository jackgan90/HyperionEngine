#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
FMesh LoadGltfPrimitive(const std::filesystem::path& InPath, std::size_t InMesh = 0, std::size_t InPrimitive = 0);
void RegisterGltfImporter(FAssetImportService& InImports);
} // namespace Hyperion
