#include "Hyperion/Materials/MaterialAsset.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FMaterialParameterType>()
{
	static const auto Type = MakeRecord<FMaterialParameterType>(
	    "hyperion.materialparametertype",
	    {Member("kind", &FMaterialParameterType::Kind), Member("scalar", &FMaterialParameterType::Scalar),
	     Member("rows", &FMaterialParameterType::Rows), Member("columns", &FMaterialParameterType::Columns),
	     Member("arrayCount", &FMaterialParameterType::ArrayCount),
	     Member("memberNames", &FMaterialParameterType::MemberNames),
	     Member("members", &FMaterialParameterType::Members)},
	    1,
	    [](const FMaterialParameterType& InValue)
	    {
		    InValue.Validate();
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialSampler>()
{
	static const auto Type = MakeRecord<FMaterialSampler>(
	    "hyperion.materialsampler",
	    {Member("u", &FMaterialSampler::U), Member("v", &FMaterialSampler::V), Member("w", &FMaterialSampler::W),
	     Member("minLinear", &FMaterialSampler::bMinLinear), Member("magLinear", &FMaterialSampler::bMagLinear),
	     Member("mipLinear", &FMaterialSampler::bMipLinear), Member("comparison", &FMaterialSampler::bComparison),
	     Member("maxAnisotropy", &FMaterialSampler::MaxAnisotropy), Member("mipLodBias", &FMaterialSampler::MipLodBias),
	     Member("minLod", &FMaterialSampler::MinLod), Member("maxLod", &FMaterialSampler::MaxLod),
	     Member("borderColor", &FMaterialSampler::BorderColor), Member("compare", &FMaterialSampler::Compare)},
	    1,
	    [](const FMaterialSampler& InValue)
	    {
		    InValue.Validate();
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialAssetValue>()
{
	static const auto Type = MakeRecord<FMaterialAssetValue>(
	    "hyperion.materialassetvalue",
	    {Member("type", &FMaterialAssetValue::Type), Member("words", &FMaterialAssetValue::Words),
	     Member("elements", &FMaterialAssetValue::Elements), Member("texture", &FMaterialAssetValue::Texture),
	     Member("sampler", &FMaterialAssetValue::Sampler)},
	    1, ValidateMaterialAssetValue);
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialAssetParameter>()
{
	static const auto Type = MakeRecord<FMaterialAssetParameter>(
	    "hyperion.materialassetparameter",
	    {Member("name", &FMaterialAssetParameter::Name), Member("type", &FMaterialAssetParameter::Type),
	     Member("semantic", &FMaterialAssetParameter::Semantic), Member("targets", &FMaterialAssetParameter::Targets),
	     Member("source", &FMaterialAssetParameter::Source),
	     Member("overridePolicy", &FMaterialAssetParameter::OverridePolicy),
	     Member("overrideScopes", &FMaterialAssetParameter::OverrideScopes),
	     Member("required", &FMaterialAssetParameter::bRequired), Member("active", &FMaterialAssetParameter::bActive),
	     Member("default", &FMaterialAssetParameter::Default)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialAssetEntry>()
{
	static const auto Type =
	    MakeRecord<FMaterialAssetEntry>("hyperion.materialassetentry", {Member("name", &FMaterialAssetEntry::Name),
	                                                                    Member("value", &FMaterialAssetEntry::Value)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialAsset>()
{
	static const auto Type = MakeRecord<FMaterialAsset>(
	    "hyperion.materialasset",
	    {Member("name", &FMaterialAsset::Name), Member("version", &FMaterialAsset::Version),
	     Member("passes", &FMaterialAsset::Passes), Member("parameters", &FMaterialAsset::Parameters),
	     Member("values", &FMaterialAsset::Values)},
	    1, ValidateMaterialAsset);
	return Type;
}
} // namespace Hyperion
