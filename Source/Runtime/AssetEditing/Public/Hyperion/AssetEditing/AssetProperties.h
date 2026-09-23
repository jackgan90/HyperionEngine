#pragma once
#include "Hyperion/AssetEditing/AssetDocument.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
struct FAssetReferenceRequirement
{
	FAssetRef Reference;
	std::optional<ETextureDimension> Dimension;
};

struct FPreparedAssetField
{
	FArchiveNode Value;
	std::vector<FAssetReferenceRequirement> References;
};

bool CanEditMaterialParameter(const FMaterialAssetParameter& InParameter);
void ClampEditableMaterialParameter(std::string_view InSemantic, FMaterialAssetValue& InValue);
FPreparedAssetField PrepareAssetField(const FAssetEditDocument& InDocument, std::string_view InField,
                                      FArchiveNode InValue);
void ValidateAssetReferenceGraph(const FAssetGraph& InGraph, std::string_view InType,
                                 std::optional<ETextureDimension> InDimension = {});
void CommitAssetField(FAssetEditDocument& InDocument, std::string InField, FArchiveNode InValue,
                      std::uint64_t InInteraction = 0, bool bInAffectsPreview = true);
} // namespace Hyperion
