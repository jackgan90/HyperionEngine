#pragma once
#include "Hyperion/AssetEditing/AssetPreviewOptions.h"
#include "Hyperion/Tasks/AsyncResult.h"
#include "Hyperion/Textures/TextureAsset.h"

namespace Hyperion
{
FTextureAsset BuildAssetTextureDisplay(const FTextureAsset& InSource, std::size_t InMip, std::size_t InFace,
                                       EAssetPreviewChannel InChannel, float InExposure, bool bInChecker,
                                       FCancellationToken InCancellation);
} // namespace Hyperion
