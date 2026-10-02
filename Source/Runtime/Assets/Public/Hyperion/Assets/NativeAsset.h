#pragma once
#include "Hyperion/AssetTypes/AssetTypes.h"
#include "Hyperion/Serialization/Archive.h"

namespace Hyperion
{
struct FAssetDocument
{
	FAssetHeader Header;
	FArchiveNode Object;
	bool bLegacy{};
	std::size_t StoredBytes{};
};

struct FEncodedAsset
{
	FAssetHeader Header;
	std::vector<std::byte> Bytes;
};

struct FNativeAssetPayloadRange
{
	std::size_t Offset{};
	std::size_t Size{};
};

std::size_t GetNativeAssetPrefixSize();
// Validates a current native prefix and its declared file budget; does not verify payload availability or integrity.
FNativeAssetPayloadRange ProbeNativeAssetPayload(std::span<const std::byte> InPrefix, FArchiveLimits InLimits = {});

FEncodedAsset EncodeAsset(const FRecordDescriptor& InType, const void* InObject, FAssetHeader InHeader = {},
                          FArchiveLimits InLimits = {});
FAssetDocument DecodeAsset(std::shared_ptr<const std::vector<std::byte>> InBytes, FArchiveLimits InLimits = {});
FAssetDocument DecodeAsset(std::span<const std::byte> InBytes, FArchiveLimits InLimits = {});
} // namespace Hyperion
