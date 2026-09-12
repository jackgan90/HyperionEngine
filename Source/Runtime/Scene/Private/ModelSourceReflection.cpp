#include "Hyperion/Scene/ModelSource.h"

namespace Hyperion
{
template<> std::span<const EAlphaMode> RecordEnumValues<EAlphaMode>()
{
	static constexpr std::array Values{EAlphaMode::Opaque, EAlphaMode::Mask, EAlphaMode::Blend};
	return Values;
}

template<> std::span<const EWrapMode> RecordEnumValues<EWrapMode>()
{
	static constexpr std::array Values{EWrapMode::Repeat, EWrapMode::Clamp, EWrapMode::Mirror};
	return Values;
}

template<> std::span<const ESamplerFilter> RecordEnumValues<ESamplerFilter>()
{
	static constexpr std::array Values{ESamplerFilter::Nearest,           ESamplerFilter::Linear,
	                                   ESamplerFilter::NearestMipNearest, ESamplerFilter::LinearMipNearest,
	                                   ESamplerFilter::NearestMipLinear,  ESamplerFilter::LinearMipLinear};
	return Values;
}

template<> const FRecordDescriptor& RecordType<FModelSampler>()
{
	static const auto Type = MakeRecord<FModelSampler>(
	    "hyperion.modelsampler", {Member("wrapU", &FModelSampler::WrapU), Member("wrapV", &FModelSampler::WrapV),
	                              Member("min", &FModelSampler::Min), Member("mag", &FModelSampler::Mag)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FTextureBinding>()
{
	static const auto Type =
	    MakeRecord<FTextureBinding>("hyperion.texturebinding", {Member("image", &FTextureBinding::Image),
	                                                            Member("sampler", &FTextureBinding::Sampler),
	                                                            Member("texCoord", &FTextureBinding::TexCoord)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FModelImage>()
{
	static const auto Type = MakeRecord<FModelImage>(
	    "hyperion.modelimage", {Member("name", &FModelImage::Name), Member("width", &FModelImage::Width),
	                            Member("height", &FModelImage::Height), Member("rgba", &FModelImage::Rgba)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FModelMaterial>()
{
	static const auto Type = MakeRecord<FModelMaterial>(
	    "hyperion.modelmaterial",
	    {Member("name", &FModelMaterial::Name), Member("baseColor", &FModelMaterial::BaseColor),
	     Member("emissive", &FModelMaterial::Emissive), Member("metallic", &FModelMaterial::Metallic),
	     Member("roughness", &FModelMaterial::Roughness), Member("normalScale", &FModelMaterial::NormalScale),
	     Member("occlusionStrength", &FModelMaterial::OcclusionStrength),
	     Member("alphaCutoff", &FModelMaterial::AlphaCutoff), Member("alphaMode", &FModelMaterial::AlphaMode),
	     Member("doubleSided", &FModelMaterial::bDoubleSided), Member("unlit", &FModelMaterial::bUnlit),
	     Member("baseColorTexture", &FModelMaterial::BaseColorTexture),
	     Member("metallicRoughnessTexture", &FModelMaterial::MetallicRoughnessTexture),
	     Member("normalTexture", &FModelMaterial::NormalTexture),
	     Member("occlusionTexture", &FModelMaterial::OcclusionTexture),
	     Member("emissiveTexture", &FModelMaterial::EmissiveTexture)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FModelSource>()
{
	static const auto Type = MakeRecord<FModelSource>(
	    "hyperion.modelsource",
	    {Member("name", &FModelSource::Name), Member("primitives", &FModelSource::Primitives),
	     Member("materials", &FModelSource::Materials), Member("images", &FModelSource::Images),
	     Member("samplers", &FModelSource::Samplers), Member("nodes", &FModelSource::Nodes),
	     Member("roots", &FModelSource::Roots), Member("diagnostics", &FModelSource::Diagnostics)},
	    1, ValidateModelSource);
	return Type;
}
} // namespace Hyperion
