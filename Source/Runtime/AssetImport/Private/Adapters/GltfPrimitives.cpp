#include "GltfImportInternal.h"
#include <numeric>

namespace Hyperion::Private
{
namespace
{
std::vector<std::uint32_t> Triangulate(const cgltf_primitive& InSource, std::size_t InCount)
{
	std::vector<std::uint32_t> Result;
	auto SourceIndices = Indices(InSource.indices, InCount);
	if (InSource.type == cgltf_primitive_type_triangles)
	{
		Require(SourceIndices.size() % 3 == 0, "incomplete triangles");
		Result = std::move(SourceIndices);
	}
	else
	{
		for (std::size_t Index = 2; Index < SourceIndices.size(); ++Index)
		{
			const auto A = InSource.type == cgltf_primitive_type_triangle_fan ? SourceIndices[0]
			                                                                  : SourceIndices[Index - 2 + (Index & 1)];
			const auto B = InSource.type == cgltf_primitive_type_triangle_fan ? SourceIndices[Index - 1]
			                                                                  : SourceIndices[Index - 1 - (Index & 1)];
			Result.insert(Result.end(), {A, B, SourceIndices[Index]});
		}
	}
	return Result;
}

void ExpandCorners(std::vector<float>& InValues, std::size_t InComponents, std::span<const std::uint32_t> InIndices)
{
	if (InValues.empty())
	{
		return;
	}
	Require(InIndices.size() <= MaxImportBytes / sizeof(float) / InComponents, "expanded geometry exceeds budget");
	std::vector<float> Expanded;
	Expanded.reserve(InIndices.size() * InComponents);
	for (auto Index : InIndices)
	{
		Expanded.insert(Expanded.end(), InValues.begin() + Index * InComponents,
		                InValues.begin() + (Index + 1) * InComponents);
	}
	InValues = std::move(Expanded);
}

void SplitFlatCorners(FModelPrimitive& InPrimitive)
{
	ExpandCorners(InPrimitive.Positions, 3, InPrimitive.Indices);
	ExpandCorners(InPrimitive.Tangents, 4, InPrimitive.Indices);
	ExpandCorners(InPrimitive.Colors, 4, InPrimitive.Indices);
	ExpandCorners(InPrimitive.TexCoords0, 2, InPrimitive.Indices);
	ExpandCorners(InPrimitive.TexCoords1, 2, InPrimitive.Indices);
	std::iota(InPrimitive.Indices.begin(), InPrimitive.Indices.end(), 0u);
}

std::uint32_t TangentUvSet(const FModelPrimitive& InPrimitive, const FModelAsset& InModel)
{
	std::uint32_t TangentUv{};
	if (InPrimitive.Material >= 0)
	{
		const auto& Material = InModel.Materials[InPrimitive.Material];
		TangentUv = Material.NormalTexture.TexCoord;
		for (auto View : {Material.BaseColorTexture, Material.MetallicRoughnessTexture, Material.NormalTexture,
		                  Material.OcclusionTexture, Material.EmissiveTexture})
		{
			Require(View.Image < 0 || !(View.TexCoord ? InPrimitive.TexCoords1 : InPrimitive.TexCoords0).empty(),
			        "material requires a missing UV set");
		}
	}
	return TangentUv;
}

} // namespace

FModelPrimitive ConvertPrimitive(const cgltf_primitive& InSource, const cgltf_data& InData, const FModelAsset& InModel,
                                 const char* InName, std::size_t& InTotal)
{
	Require(!InSource.targets_count, "morph targets are outside the static importer");
	Require(InSource.type == cgltf_primitive_type_triangles || InSource.type == cgltf_primitive_type_triangle_strip ||
	            InSource.type == cgltf_primitive_type_triangle_fan,
	        "unsupported primitive topology");
	const auto* Positions = cgltf_find_accessor(&InSource, cgltf_attribute_type_position, 0);
	Require(Positions && Positions->count && Positions->count <= 8000000, "missing or excessive positions");
	const auto Count = Positions->count;
	FModelPrimitive Primitive;
	Primitive.Name = Name(InName);
	Primitive.Material = InSource.material ? static_cast<std::int32_t>(InSource.material - InData.materials) : -1;
	Primitive.Positions = Attribute(InSource, cgltf_attribute_type_position, 0, 3, Count);
	Primitive.Normals = Attribute(InSource, cgltf_attribute_type_normal, 0, 3, Count);
	Primitive.Tangents = Attribute(InSource, cgltf_attribute_type_tangent, 0, 4, Count);
	Primitive.Colors = Attribute(InSource, cgltf_attribute_type_color, 0, 4, Count);
	Primitive.TexCoords0 = Attribute(InSource, cgltf_attribute_type_texcoord, 0, 2, Count);
	Primitive.TexCoords1 = Attribute(InSource, cgltf_attribute_type_texcoord, 1, 2, Count);

	Primitive.Indices = Triangulate(InSource, Count);
	// glTF's absent normals imply flat shading, so split shared corners.
	if (Primitive.Normals.empty())
	{
		SplitFlatCorners(Primitive);
	}
	GenerateMeshDirections(Primitive, TangentUvSet(Primitive, InModel));
	const auto GeometryBytes =
	    (Primitive.Positions.size() + Primitive.Normals.size() + Primitive.Tangents.size() + Primitive.Colors.size() +
	     Primitive.TexCoords0.size() + Primitive.TexCoords1.size() + Primitive.Indices.size()) *
	    4;
	Require(GeometryBytes <= MaxImportBytes - InTotal, "decoded geometry budget exceeded");
	InTotal += GeometryBytes;
	return Primitive;
}

} // namespace Hyperion::Private
