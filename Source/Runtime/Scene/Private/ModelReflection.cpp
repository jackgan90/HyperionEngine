#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FVec2>()
{
	static const auto Type = MakeRecord<FVec2>("hyperion.vec2", {Member("x", &FVec2::X), Member("y", &FVec2::Y)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FVec3>()
{
	static const auto Type =
	    MakeRecord<FVec3>("hyperion.vec3", {Member("x", &FVec3::X), Member("y", &FVec3::Y), Member("z", &FVec3::Z)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FVec4>()
{
	static const auto Type = MakeRecord<FVec4>("hyperion.vec4", {Member("x", &FVec4::X), Member("y", &FVec4::Y),
	                                                             Member("z", &FVec4::Z), Member("w", &FVec4::W)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMat4>()
{
	static const auto Type = MakeRecord<FMat4>("hyperion.mat4", {Member("values", &FMat4::Values)});
	return Type;
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
	     Member("doubleSided", &FModelMaterial::DoubleSided), Member("unlit", &FModelMaterial::Unlit),
	     Member("baseColorTexture", &FModelMaterial::BaseColorTexture),
	     Member("metallicRoughnessTexture", &FModelMaterial::MetallicRoughnessTexture),
	     Member("normalTexture", &FModelMaterial::NormalTexture),
	     Member("occlusionTexture", &FModelMaterial::OcclusionTexture),
	     Member("emissiveTexture", &FModelMaterial::EmissiveTexture)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FModelPrimitive>()
{
	static const auto Type = MakeRecord<FModelPrimitive>(
	    "hyperion.modelprimitive",
	    {Member("name", &FModelPrimitive::Name), Member("positions", &FModelPrimitive::Positions),
	     Member("normals", &FModelPrimitive::Normals), Member("tangents", &FModelPrimitive::Tangents),
	     Member("colors", &FModelPrimitive::Colors), Member("texCoords0", &FModelPrimitive::TexCoords0),
	     Member("texCoords1", &FModelPrimitive::TexCoords1), Member("indices", &FModelPrimitive::Indices),
	     Member("material", &FModelPrimitive::Material)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FModelNode>()
{
	static const auto Type = MakeRecord<FModelNode>(
	    "hyperion.modelnode",
	    {Member("name", &FModelNode::Name), Member("local", &FModelNode::Local),
	     Member("primitives", &FModelNode::Primitives), Member("children", &FModelNode::Children)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FModelAsset>()
{
	static const auto Type = MakeRecord<FModelAsset>(
	    "hyperion.modelasset",
	    {Member("name", &FModelAsset::Name), Member("primitives", &FModelAsset::Primitives),
	     Member("materials", &FModelAsset::Materials), Member("images", &FModelAsset::Images),
	     Member("samplers", &FModelAsset::Samplers), Member("nodes", &FModelAsset::Nodes),
	     Member("roots", &FModelAsset::Roots), Member("diagnostics", &FModelAsset::Diagnostics)},
	    1, ValidateModel);
	return Type;
}
} // namespace Hyperion
