#include "hyperion/Assets.h"
#include "hyperion/Core.h"
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <limits>
#include <memory>
#include <stdexcept>

namespace Hyperion
{
FMesh LoadGltfPrimitive(const std::filesystem::path& InPath, std::size_t InMeshIndex, std::size_t InPrimitiveIndex)
{
	cgltf_options Options{};
	Options.memory.alloc_func = [](void*, cgltf_size InSize) -> void*
	{
		try
		{
			return Allocate(InSize, alignof(std::max_align_t), EMemoryTag::Assets);
		}
		catch (...)
		{
			return nullptr;
		}
	};
	Options.memory.free_func = [](void*, void* InPtr)
	{
		Deallocate(InPtr);
	};
	cgltf_data* Raw{};
	auto Name = InPath.string();
	if (cgltf_parse_file(&Options, Name.c_str(), &Raw) != cgltf_result_success)
	{
		throw std::runtime_error("glTF parse failed: " + Name);
	}
	std::unique_ptr<cgltf_data, decltype(&cgltf_free)> Data(Raw, cgltf_free);
	if (cgltf_load_buffers(&Options, Raw, Name.c_str()) != cgltf_result_success ||
	    cgltf_validate(Raw) != cgltf_result_success)
	{
		throw std::runtime_error("glTF buffer validation failed: " + Name);
	}
	if (InMeshIndex >= Raw->meshes_count || InPrimitiveIndex >= Raw->meshes[InMeshIndex].primitives_count)
	{
		throw std::out_of_range("glTF primitive");
	}
	const auto& Primitive = Raw->meshes[InMeshIndex].primitives[InPrimitiveIndex];
	if (Primitive.type != cgltf_primitive_type_triangles)
	{
		throw std::runtime_error("Only triangle primitives are supported");
	}
	const cgltf_accessor* Positions{};
	const cgltf_accessor* Colors{};
	const cgltf_accessor* Uvs{};
	for (cgltf_size I = 0; I < Primitive.attributes_count; ++I)
	{
		const auto& A = Primitive.attributes[I];
		if (A.type == cgltf_attribute_type_position)
		{
			Positions = A.data;
		}
		if (A.type == cgltf_attribute_type_color && A.index == 0)
		{
			Colors = A.data;
		}
		if (A.type == cgltf_attribute_type_texcoord && A.index == 0)
		{
			Uvs = A.data;
		}
	}
	if (!Positions || Positions->type != cgltf_type_vec3 ||
	    Positions->count > std::numeric_limits<std::uint32_t>::max())
	{
		throw std::runtime_error("Invalid glTF positions");
	}
	FMesh Result;
	Result.Vertices.resize(Positions->count);
	for (std::size_t I = 0; I < Result.Vertices.size(); ++I)
	{
		float P[3]{};
		float C[4]{1, 1, 1, 1};
		float Uv[2]{};
		if (!cgltf_accessor_read_float(Positions, I, P, 3))
		{
			throw std::runtime_error("Invalid position accessor");
		}
		if (Colors && !cgltf_accessor_read_float(Colors, I, C, 4))
		{
			throw std::runtime_error("Invalid color accessor");
		}
		if (Uvs && !cgltf_accessor_read_float(Uvs, I, Uv, 2))
		{
			throw std::runtime_error("Invalid UV accessor");
		}
		Result.Vertices[I] = {{P[0], P[1], P[2]}, {C[0], C[1], C[2], C[3]}, {Uv[0], Uv[1]}};
	}
	auto Count = Primitive.indices ? Primitive.indices->count : Positions->count;
	if (Count % 3)
	{
		throw std::runtime_error("Incomplete glTF triangles");
	}
	Result.Indices.reserve(Count);
	for (std::size_t I = 0; I < Count; ++I)
	{
		auto Index = Primitive.indices ? cgltf_accessor_read_index(Primitive.indices, I) : I;
		if (Index >= Result.Vertices.size())
		{
			throw std::runtime_error("glTF index out of bounds");
		}
		Result.Indices.push_back(static_cast<std::uint32_t>(Index));
	}
	return Result;
}
} // namespace Hyperion
