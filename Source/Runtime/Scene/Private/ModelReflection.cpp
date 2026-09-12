#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FMaterialOverride>()
{
	static const auto Type = MakeRecord<FMaterialOverride>("hyperion.materialoverride",
	                                                       {Member("baseColor", &FMaterialOverride::BaseColor),
	                                                        Member("metallic", &FMaterialOverride::Metallic),
	                                                        Member("roughness", &FMaterialOverride::Roughness)},
	                                                       1, ValidateMaterialOverride);
	return Type;
}

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
	static const auto Type = []
	{
		auto Result = MakeRecord<FModelAsset>(
		    "hyperion.modelasset",
		    {Member("name", &FModelAsset::Name), Member("primitives", &FModelAsset::Primitives),
		     Member("materialSlots", &FModelAsset::MaterialSlots), Member("nodes", &FModelAsset::Nodes),
		     Member("roots", &FModelAsset::Roots), Member("diagnostics", &FModelAsset::Diagnostics)},
		    2, ValidateModel);
		Result.Migrations.emplace(
		    1,
		    [](FArchiveNode::FObject&)
		    {
			    throw std::runtime_error(
			        "Embedded model schema 1 requires AssetTool upgrade to independent material/texture assets");
		    });
		return Result;
	}();
	return Type;
}
} // namespace Hyperion
