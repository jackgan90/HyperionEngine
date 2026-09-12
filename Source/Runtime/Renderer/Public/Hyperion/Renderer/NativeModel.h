#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Scene/Scene.h"

namespace Hyperion
{
class FRenderResourceService;

// The graph pins complete immutable native revisions. All IO stays in the generic asset service.
std::shared_ptr<const FMaterialAssetData> ResolveMaterialAssetGraph(const FAssetGraph& InGraph,
                                                                    const FLoadedAsset& InMaterial,
                                                                    const FAssetService& InAssets);
std::shared_ptr<const FSceneModelData> ResolveModelAssetGraph(const FAssetGraph& InGraph,
                                                              const FAssetService& InAssets);
TAsyncResult<FSceneModelData> LoadNativeModel(FAssetService& InAssets, FTaskSystem& InTasks,
                                              const FAssetRef& InReference,
                                              const std::filesystem::path& InContainingAsset,
                                              FCancellationToken InCancellation = {},
                                              FRenderResourceService* InResources = nullptr);
TAsyncResult<FSceneModelData> LoadNativeModel(FAssetService& InAssets, FTaskSystem& InTasks,
                                              const std::filesystem::path& InPath,
                                              FCancellationToken InCancellation = {},
                                              FRenderResourceService* InResources = nullptr);
} // namespace Hyperion
