#pragma once
#include "Hyperion/Renderer/NativeModel.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "Hyperion/Scene/SceneManifest.h"

namespace Hyperion
{
TAsyncResult<FSceneMaterialSelection> LoadSceneMaterialSelection(FAssetService& InAssets, FTaskSystem& InTasks,
                                                                 FRenderResourceService& InResources,
                                                                 FSceneMaterialAsset InSelection,
                                                                 std::filesystem::path InContainingAsset,
                                                                 FCancellationToken InCancellation = {});
FSceneMaterialAsset PersistSceneMaterialSelection(const FSceneMaterialSelection& InSelection);
} // namespace Hyperion
