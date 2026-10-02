#pragma once
#include "Hyperion/AssetEditing/AssetDocument.h"
#include "Hyperion/AssetEditing/AssetFieldPolicy.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
bool CanEditMaterialParameter(const FMaterialAssetParameter& InParameter);
void ClampEditableMaterialParameter(std::string_view InSemantic, FMaterialAssetValue& InValue);
void ValidateAssetReferenceGraph(const FAssetGraph& InGraph, std::string_view InType,
                                 std::optional<ETextureDimension> InDimension = {});
void CommitAssetField(FAssetEditDocument& InDocument, std::string InField, FArchiveNode InValue,
                      std::uint64_t InInteraction = 0);
void CommitAssetField(FAssetEditDocument& InDocument, const FRecordMemberIdentity& InField, FArchiveNode InValue,
                      std::uint64_t InInteraction = 0);
} // namespace Hyperion
