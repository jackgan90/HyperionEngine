#pragma once
#include "Hyperion/AssetTypes/AssetTypes.h"
#include "Hyperion/Materials/Material.h"

namespace Hyperion
{
struct FMaterialAssetValue
{
	FMaterialParameterType Type;
	std::vector<std::uint32_t> Words;
	std::vector<FMaterialAssetValue> Elements;
	std::optional<FAssetRef> Texture;
	FMaterialSampler Sampler;
	bool operator==(const FMaterialAssetValue&) const = default;
};

struct FMaterialAssetParameter
{
	std::string Name;
	FMaterialParameterType Type;
	std::string Semantic;
	std::vector<std::string> Targets;
	EMaterialParameterSource Source = EMaterialParameterSource::Manual;
	EMaterialOverridePolicy OverridePolicy = EMaterialOverridePolicy::AllowOverride;
	std::uint32_t OverrideScopes = MaterialScopeBit(EMaterialScope::Material) |
	                               MaterialScopeBit(EMaterialScope::Object) | MaterialScopeBit(EMaterialScope::Draw);
	bool bRequired = true;
	bool bActive = true;
	std::optional<FMaterialAssetValue> Default;
};

struct FMaterialAssetEntry
{
	std::string Name;
	FMaterialAssetValue Value;
	bool operator==(const FMaterialAssetEntry&) const = default;
};

using FMaterialAssetValues = std::vector<FMaterialAssetEntry>;

struct FMaterialAsset
{
	std::string Name;
	std::uint64_t Version = 1;
	std::vector<FMaterialPass> Passes;
	std::vector<FMaterialAssetParameter> Parameters;
	FMaterialAssetValues Values;
};

struct FMaterialAssetData
{
	std::shared_ptr<const FMaterialAsset> Asset;
	std::map<FAssetRef, std::shared_ptr<const FTextureAsset>> Textures;
};

using FMaterialTextureResolver = std::function<std::shared_ptr<const FMaterialTextureSource>(const FAssetRef&)>;
using FMaterialTextureReference = std::function<FAssetRef(const std::shared_ptr<const FMaterialTextureSource>&)>;

void ValidateMaterialAssetValue(const FMaterialAssetValue& InValue);
void ValidateMaterialAsset(const FMaterialAsset& InAsset);
FMaterialAssetValue PersistMaterialValue(const FMaterialValue& InValue,
                                         const FMaterialTextureReference& InReference = {});
FMaterialValue ResolveMaterialAssetValue(const FMaterialAssetValue& InValue,
                                         const FMaterialTextureResolver& InResolve = {});
FMaterialAsset PersistMaterialDescription(const FMaterialDescription& InDescription,
                                          const FMaterialTextureReference& InReference = {});
FMaterialDescription ResolveMaterialAssetDescription(const FMaterialAsset& InAsset,
                                                     const FMaterialTextureResolver& InResolve = {});
FMaterialParameterValues ResolveMaterialAssetValues(const FMaterialAssetValues& InValues,
                                                    const FMaterialTextureResolver& InResolve = {});

template<> const FRecordDescriptor& RecordType<FMaterialAssetValue>();
template<> const FRecordDescriptor& RecordType<FMaterialAssetParameter>();
template<> const FRecordDescriptor& RecordType<FMaterialAssetEntry>();
template<> const FRecordDescriptor& RecordType<FMaterialAsset>();
template<> const FRecordDescriptor& RecordType<FMaterialParameterType>();
template<> const FRecordDescriptor& RecordType<FMaterialSampler>();
template<> const FRecordDescriptor& RecordType<FMaterialStencilFace>();
template<> const FRecordDescriptor& RecordType<FMaterialState>();
template<> const FRecordDescriptor& RecordType<FMaterialDynamicState>();
template<> const FRecordDescriptor& RecordType<FMaterialShaderDefine>();
template<> const FRecordDescriptor& RecordType<FMaterialShader>();
template<> const FRecordDescriptor& RecordType<FMaterialInstanceArray>();
template<> const FRecordDescriptor& RecordType<FMaterialPass>();
template<> std::span<const EMaterialValueKind> RecordEnumValues<EMaterialValueKind>();
template<> std::span<const EMaterialScalar> RecordEnumValues<EMaterialScalar>();
template<> std::span<const EMaterialParameterSource> RecordEnumValues<EMaterialParameterSource>();
template<> std::span<const EMaterialOverridePolicy> RecordEnumValues<EMaterialOverridePolicy>();
template<> std::span<const EMaterialAddressMode> RecordEnumValues<EMaterialAddressMode>();
template<> std::span<const EMaterialSamplerCompare> RecordEnumValues<EMaterialSamplerCompare>();
template<> std::span<const EMaterialFill> RecordEnumValues<EMaterialFill>();
template<> std::span<const EMaterialCull> RecordEnumValues<EMaterialCull>();
template<> std::span<const EMaterialCompare> RecordEnumValues<EMaterialCompare>();
template<> std::span<const EMaterialStencilOp> RecordEnumValues<EMaterialStencilOp>();
template<> std::span<const EMaterialBlendFactor> RecordEnumValues<EMaterialBlendFactor>();
template<> std::span<const EMaterialBlendOp> RecordEnumValues<EMaterialBlendOp>();
template<> std::span<const EMaterialQueue> RecordEnumValues<EMaterialQueue>();
} // namespace Hyperion
