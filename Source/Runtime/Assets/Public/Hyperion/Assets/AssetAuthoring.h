#pragma once
#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/IO/IOService.h"

namespace Hyperion
{
std::filesystem::path AssetProductPath(const FRecordDescriptor& InType, const void* InObject,
                                       const std::filesystem::path& InRoot, std::string_view InId);
// Mutates only an exclusively owned authoring snapshot.
void PrepareAssetReferences(const FRecordDescriptor& InType, void* InObject, IFileSystem& InFiles,
                            const std::filesystem::path& InOutput);
} // namespace Hyperion
