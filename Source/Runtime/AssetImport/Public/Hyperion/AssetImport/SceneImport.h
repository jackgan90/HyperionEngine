#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"
#include "Hyperion/Scene/SceneManifest.h"

namespace Hyperion
{
FSceneManifest DecodeSceneManifest(std::string_view InText);
void RegisterSceneImporter(FAssetImportService& InImports);
} // namespace Hyperion
