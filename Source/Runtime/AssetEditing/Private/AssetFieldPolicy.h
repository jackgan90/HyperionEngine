#pragma once
#include "Hyperion/AssetEditing/AssetProperties.h"

namespace Hyperion
{
struct FAssetReferenceRequirement
{
	FAssetRef Reference;
	std::optional<ETextureDimension> Dimension;
	bool operator==(const FAssetReferenceRequirement&) const = default;
};

struct FPreparedAssetField
{
	FArchiveNode Value;
	std::vector<FAssetReferenceRequirement> References;
};

FPreparedAssetField PrepareAssetField(const FAssetEditDocument& InDocument, const FRecordMemberIdentity& InField,
                                      FArchiveNode InValue);
bool AssetFieldAffectsPreview(const FAssetEditDocument& InDocument, const FRecordMemberIdentity& InField,
                              const FArchiveNode& InValue);
} // namespace Hyperion
