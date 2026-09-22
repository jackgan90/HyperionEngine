#pragma once
#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/IO/IOService.h"

namespace Hyperion
{
class FAssetService;

struct FAssetRegistryEntry
{
	std::filesystem::path Path;
	FAssetHeader Header;
};

struct FAssetDiscovery
{
	std::vector<FAssetRegistryEntry> Entries;
	std::map<std::filesystem::path, std::string> Errors;
};

// Metadata discovery is not payload integrity validation; actual loads still decode the full container.
FAssetHeader ReadAssetHeader(IFileSystem& InFiles, const std::filesystem::path& InPath);
FAssetDiscovery DiscoverAssets(IFileSystem& InFiles, const std::filesystem::path& InRoot);
std::vector<FAssetRef> BuildAssetIndex(std::span<const FAssetRegistryEntry> InEntries);
// Synchronous discovery for native graph tools: all mounts plus any unmounted local library.
// Unreadable headers stay unavailable; loading a graph reports failures on its actual dependencies.
void IndexDiscoveredAssets(FAssetService& InAssets, const std::filesystem::path& InLocalRoot);
} // namespace Hyperion
