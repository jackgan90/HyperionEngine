#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <cgltf.h>
#include <cstring>
#include <numeric>

namespace Hyperion
{
namespace
{
constexpr std::size_t MaxImportBytes = 512u * 1024u * 1024u;

void Require(bool InValue, const std::string& InMessage)
{
	if (!InValue)
	{
		throw std::runtime_error("glTF: " + InMessage);
	}
}

std::string Name(const char* InName)
{
	return InName ? InName : "";
}

FBytes Base64(std::string_view InText)
{
	Require(InText.size() <= MaxImportBytes && InText.size() % 4 == 0, "invalid base64 length");
	const std::string Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	FBytes Bytes;
	Bytes.reserve(InText.size() / 4 * 3);
	for (std::size_t Index = 0; Index < InText.size(); Index += 4)
	{
		std::uint32_t Value{};
		unsigned Padding{};
		for (unsigned Offset = 0; Offset < 4; ++Offset)
		{
			const char Character = InText[Index + Offset];
			if (Character == '=')
			{
				Require(Index + 4 == InText.size() && Offset >= 2, "invalid base64 padding");
				++Padding;
				Value <<= 6;
			}
			else
			{
				const auto Digit = Alphabet.find(Character);
				Require(!Padding && Digit != std::string::npos, "invalid base64 digit");
				Value = (Value << 6) | static_cast<std::uint32_t>(Digit);
			}
		}
		Bytes.push_back(static_cast<std::byte>((Value >> 16) & 255));
		if (Padding < 2)
		{
			Bytes.push_back(static_cast<std::byte>((Value >> 8) & 255));
		}
		if (Padding < 1)
		{
			Bytes.push_back(static_cast<std::byte>(Value & 255));
		}
	}
	return Bytes;
}

std::shared_ptr<const FBytes> ReadUri(FAssetLoadContext& InContext, const char* InUri)
{
	Require(InUri != nullptr, "missing dependency URI");
	const std::string Uri(InUri);
	if (Uri.starts_with("data:"))
	{
		const auto Comma = Uri.find(',');
		Require(Comma != std::string::npos && Uri.substr(0, Comma).ends_with(";base64"),
		        "only base64 data URIs are supported");
		return std::make_shared<FBytes>(Base64(std::string_view(Uri).substr(Comma + 1)));
	}
	Require(Uri.find("://") == std::string::npos && Uri.find('\0') == std::string::npos, "unsupported URI scheme");
	std::string Decoded;
	for (std::size_t Index = 0; Index < Uri.size(); ++Index)
	{
		if (Uri[Index] != '%')
		{
			Decoded += Uri[Index];
			continue;
		}
		Require(Index + 2 < Uri.size(), "invalid URI escape");
		unsigned Value{};
		for (unsigned Offset = 1; Offset <= 2; ++Offset)
		{
			const auto Character = Uri[Index + Offset];
			const int Digit = Character >= '0' && Character <= '9'   ? Character - '0'
			                  : Character >= 'a' && Character <= 'f' ? Character - 'a' + 10
			                  : Character >= 'A' && Character <= 'F' ? Character - 'A' + 10
			                                                         : -1;
			Require(Digit >= 0, "invalid URI escape");
			Value = Value * 16 + static_cast<unsigned>(Digit);
		}
		Require(Value != 0, "NUL in dependency path");
		Decoded += static_cast<char>(Value);
		Index += 2;
	}
	const std::u8string Utf8(reinterpret_cast<const char8_t*>(Decoded.data()), Decoded.size());
	return InContext.Read((InContext.Path.parent_path() / std::filesystem::path(Utf8)).lexically_normal());
}

std::uint32_t Unsigned(const std::uint8_t* InData, cgltf_component_type InType);

void ValidateRange(const cgltf_buffer_view* InView, std::size_t InOffset, std::size_t InCount, std::size_t InStride,
                   std::size_t InElement)
{
	Require(InView && InCount && InElement && InStride >= InElement, "invalid accessor layout");
	Require(InOffset <= InView->size && InElement <= InView->size - InOffset, "accessor offset exceeds buffer view");
	Require(InCount - 1 <= (InView->size - InOffset - InElement) / InStride, "accessor range exceeds buffer view");
}

void ValidateAccessorRanges(const cgltf_data& InData)
{
	// cgltf_validate can read sparse indices before it validates the enclosing view.
	// Check all physical ranges first, using subtraction/division to avoid overflow.
	for (std::size_t Index = 0; Index < InData.buffer_views_count; ++Index)
	{
		const auto& View = InData.buffer_views[Index];
		Require(View.buffer && View.offset <= View.buffer->size && View.size <= View.buffer->size - View.offset,
		        "buffer view exceeds source buffer");
	}
	for (std::size_t Index = 0; Index < InData.accessors_count; ++Index)
	{
		const auto& Accessor = InData.accessors[Index];
		const auto Element = cgltf_calc_size(Accessor.type, Accessor.component_type);
		Require(Accessor.count && Accessor.count <= MaxImportBytes / sizeof(std::uint32_t) && Element,
		        "invalid or excessive accessor count/type");
		if (Accessor.buffer_view)
		{
			ValidateRange(Accessor.buffer_view, Accessor.offset, Accessor.count, Accessor.stride, Element);
		}
		else
		{
			Require(Accessor.offset == 0, "bufferless accessor has an offset");
		}
		if (Accessor.is_sparse)
		{
			const auto& Sparse = Accessor.sparse;
			Require(Sparse.count && Sparse.count <= Accessor.count, "invalid sparse count");
			Require(Sparse.indices_component_type == cgltf_component_type_r_8u ||
			            Sparse.indices_component_type == cgltf_component_type_r_16u ||
			            Sparse.indices_component_type == cgltf_component_type_r_32u,
			        "invalid sparse index component");
			const auto IndexSize = cgltf_component_size(Sparse.indices_component_type);
			ValidateRange(Sparse.indices_buffer_view, Sparse.indices_byte_offset, Sparse.count, IndexSize, IndexSize);
			ValidateRange(Sparse.values_buffer_view, Sparse.values_byte_offset, Sparse.count, Element, Element);
			const auto* Indices = cgltf_buffer_view_data(Sparse.indices_buffer_view) + Sparse.indices_byte_offset;
			std::uint32_t Previous{};
			for (std::size_t Item = 0; Item < Sparse.count; ++Item)
			{
				const auto Destination = Unsigned(Indices + Item * IndexSize, Sparse.indices_component_type);
				Require(Destination < Accessor.count && (!Item || Destination > Previous),
				        "sparse indices must be in range and strictly increasing");
				Previous = Destination;
			}
		}
	}
}

std::vector<float> Attribute(const cgltf_primitive& InPrimitive, cgltf_attribute_type InType, int InSet,
                             unsigned InComponents, std::size_t InCount)
{
	const auto* Accessor = cgltf_find_accessor(&InPrimitive, InType, InSet);
	if (!Accessor)
	{
		return {};
	}
	const auto Components = cgltf_num_components(Accessor->type);
	Require(Accessor->count == InCount &&
	            (Components == InComponents || (InType == cgltf_attribute_type_color && Components == 3)),
	        "attribute shape/count mismatch");
	Require(InCount <= MaxImportBytes / sizeof(float) / InComponents, "attribute exceeds import limit");
	std::vector<float> Values(InCount * Components);
	auto Base = *Accessor;
	Base.is_sparse = false;
	Require(cgltf_accessor_unpack_floats(&Base, Values.data(), Values.size()) == Values.size(),
	        "cannot expand accessor");
	if (Accessor->is_sparse)
	{
		const auto& Sparse = Accessor->sparse;
		auto Overlay = Base;
		Overlay.buffer_view = Sparse.values_buffer_view;
		Overlay.offset = Sparse.values_byte_offset;
		Overlay.count = Sparse.count;
		Overlay.stride = cgltf_calc_size(Accessor->type, Accessor->component_type);
		const auto* Indices = cgltf_buffer_view_data(Sparse.indices_buffer_view) + Sparse.indices_byte_offset;
		for (std::size_t Index = 0; Index < Sparse.count; ++Index)
		{
			const auto Destination = Unsigned(Indices + Index * cgltf_component_size(Sparse.indices_component_type),
			                                  Sparse.indices_component_type);
			Require(cgltf_accessor_read_float(&Overlay, Index, Values.data() + Destination * Components, Components),
			        "cannot expand sparse accessor");
		}
	}
	if (Components != InComponents)
	{
		std::vector<float> Expanded(InCount * InComponents, 1);
		for (std::size_t Index = 0; Index < InCount; ++Index)
		{
			std::copy_n(Values.data() + Index * Components, Components, Expanded.data() + Index * InComponents);
		}
		return Expanded;
	}
	return Values;
}

std::uint32_t Unsigned(const std::uint8_t* InData, cgltf_component_type InType)
{
	if (InType == cgltf_component_type_r_8u)
	{
		return *InData;
	}
	if (InType == cgltf_component_type_r_16u)
	{
		std::uint16_t Value{};
		std::memcpy(&Value, InData, 2);
		return Value;
	}
	Require(InType == cgltf_component_type_r_32u, "invalid unsigned index component");
	std::uint32_t Value{};
	std::memcpy(&Value, InData, 4);
	return Value;
}

std::vector<std::uint32_t> Indices(const cgltf_accessor* InAccessor, std::size_t InVertices)
{
	const auto Count = InAccessor ? InAccessor->count : InVertices;
	Require(Count <= MaxImportBytes / sizeof(std::uint32_t), "index count exceeds limit");
	std::vector<std::uint32_t> Values(Count);
	if (!InAccessor)
	{
		std::iota(Values.begin(), Values.end(), 0u);
		return Values;
	}
	Require(InAccessor->type == cgltf_type_scalar && !InAccessor->normalized, "invalid index accessor");
	if (InAccessor->buffer_view)
	{
		const auto* Data = cgltf_buffer_view_data(InAccessor->buffer_view) + InAccessor->offset;
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			Values[Index] = Unsigned(Data + InAccessor->stride * Index, InAccessor->component_type);
		}
	}
	if (InAccessor->is_sparse)
	{
		const auto& Sparse = InAccessor->sparse;
		const auto* SparseIndices = cgltf_buffer_view_data(Sparse.indices_buffer_view) + Sparse.indices_byte_offset;
		const auto* SparseValues = cgltf_buffer_view_data(Sparse.values_buffer_view) + Sparse.values_byte_offset;
		for (std::size_t Index = 0; Index < Sparse.count; ++Index)
		{
			const auto Destination =
			    Unsigned(SparseIndices + cgltf_component_size(Sparse.indices_component_type) * Index,
			             Sparse.indices_component_type);
			Require(Destination < Count, "sparse index out of range");
			Values[Destination] = Unsigned(SparseValues + cgltf_component_size(InAccessor->component_type) * Index,
			                               InAccessor->component_type);
		}
	}
	for (auto Value : Values)
	{
		Require(Value < InVertices, "vertex index out of bounds");
	}
	return Values;
}

EWrapMode Wrap(cgltf_wrap_mode InWrap)
{
	if (InWrap == cgltf_wrap_mode_clamp_to_edge)
	{
		return EWrapMode::Clamp;
	}
	if (InWrap == cgltf_wrap_mode_mirrored_repeat)
	{
		return EWrapMode::Mirror;
	}
	return EWrapMode::Repeat;
}

ESamplerFilter Filter(cgltf_filter_type InFilter, bool InMagnification)
{
	switch (InFilter)
	{
		case cgltf_filter_type_nearest:
			return ESamplerFilter::Nearest;
		case cgltf_filter_type_linear:
			return ESamplerFilter::Linear;
		case cgltf_filter_type_nearest_mipmap_nearest:
			return ESamplerFilter::NearestMipNearest;
		case cgltf_filter_type_linear_mipmap_nearest:
			return ESamplerFilter::LinearMipNearest;
		case cgltf_filter_type_nearest_mipmap_linear:
			return ESamplerFilter::NearestMipLinear;
		default:
			return InMagnification ? ESamplerFilter::Linear : ESamplerFilter::LinearMipLinear;
	}
}

std::shared_ptr<void> Import(FAssetLoadContext& InContext)
{
	cgltf_options Options{};
	Options.memory.alloc_func = [](void*, cgltf_size InSize) -> void*
	{
		if (InSize > MaxImportBytes)
		{
			return nullptr;
		}
		try
		{
			return Allocate(InSize, alignof(std::max_align_t), EMemoryTag::Assets);
		}
		catch (...)
		{
			return nullptr;
		}
	};
	Options.memory.free_func = [](void*, void* InPointer)
	{
		Deallocate(InPointer);
	};
	// A defensive hook ensures accidental future file-loader calls cannot bypass IO.
	Options.file.read = [](const cgltf_memory_options*, const cgltf_file_options*, const char*, cgltf_size*, void**)
	{
		return cgltf_result_io_error;
	};
	cgltf_data* Raw{};
	Require(cgltf_parse(&Options, InContext.Bytes->data(), InContext.Bytes->size(), &Raw) == cgltf_result_success,
	        "parse failed");
	std::unique_ptr<cgltf_data, decltype(&cgltf_free)> Data(Raw, cgltf_free);
	auto Model = std::static_pointer_cast<FModelAsset>(RecordType<FModelAsset>().Create());
	const auto PathName = InContext.Path.filename().u8string();
	Model->Name.assign(reinterpret_cast<const char*>(PathName.data()), PathName.size());
	Require(!Raw->animations_count && !Raw->skins_count, "animation/skinning is outside the static importer");
	for (std::size_t Index = 0; Index < Raw->extensions_required_count; ++Index)
	{
		Require(std::string_view(Raw->extensions_required[Index]) == "KHR_materials_unlit",
		        "unsupported required extension: " + Name(Raw->extensions_required[Index]));
	}
	for (std::size_t Index = 0; Index < Raw->extensions_used_count; ++Index)
	{
		if (std::string_view(Raw->extensions_used[Index]) != "KHR_materials_unlit")
		{
			Model->Diagnostics.push_back("Optional extension ignored: " + Name(Raw->extensions_used[Index]));
		}
	}
	std::vector<std::shared_ptr<const FBytes>> OwnedBuffers;
	std::size_t Total = InContext.Bytes->size();
	for (std::size_t Index = 0; Index < Raw->buffers_count; ++Index)
	{
		auto& Buffer = Raw->buffers[Index];
		if (Buffer.uri)
		{
			auto Bytes = ReadUri(InContext, Buffer.uri);
			Require(Bytes->size() >= Buffer.size && Bytes->size() <= MaxImportBytes - Total,
			        "buffer size mismatch or import budget exceeded");
			Total += Bytes->size();
			Buffer.data = const_cast<std::byte*>(Bytes->data());
			OwnedBuffers.push_back(std::move(Bytes));
		}
		else
		{
			Require(Index == 0 && Raw->bin && Raw->bin_size >= Buffer.size, "missing GLB binary chunk");
			Buffer.data = const_cast<void*>(Raw->bin);
		}
		Buffer.data_free_method = cgltf_data_free_method_none;
	}
	ValidateAccessorRanges(*Raw);
	Model->Nodes.reserve(Raw->nodes_count);
	for (std::size_t Index = 0; Index < Raw->nodes_count; ++Index)
	{
		const auto& Source = Raw->nodes[Index];
		FModelNode Node;
		Node.Name = Name(Source.name);
		cgltf_node_transform_local(&Source, Node.Local.Values.data());
		for (std::size_t Child = 0; Child < Source.children_count; ++Child)
		{
			Node.Children.push_back(static_cast<std::uint32_t>(Source.children[Child] - Raw->nodes));
		}
		Model->Nodes.push_back(std::move(Node));
	}
	// Bound depth before the vendor validator walks every parent chain.
	ValidateNodeHierarchy(Model->Nodes);
	Require(cgltf_validate(Raw) == cgltf_result_success, "buffer/accessor validation failed");
	for (std::size_t Index = 0; Index < Raw->images_count; ++Index)
	{
		InContext.Cancellation.Check();
		const auto& Image = Raw->images[Index];
		std::shared_ptr<const FBytes> External;
		std::span<const std::byte> Bytes;
		if (Image.uri)
		{
			External = ReadUri(InContext, Image.uri);
			Bytes = *External;
		}
		else
		{
			Require(Image.buffer_view != nullptr, "image has no URI or buffer view");
			Bytes = {reinterpret_cast<const std::byte*>(cgltf_buffer_view_data(Image.buffer_view)),
			         Image.buffer_view->size};
		}
		auto Pixels = DecodeImage(Bytes);
		Require(Pixels.Rgba.size() <= MaxImportBytes - Total, "decoded image budget exceeded");
		Total += Pixels.Rgba.size();
		Model->Images.push_back({Name(Image.name), Pixels.Width, Pixels.Height, std::move(Pixels.Rgba)});
	}
	for (std::size_t Index = 0; Index < Raw->samplers_count; ++Index)
	{
		const auto& Sampler = Raw->samplers[Index];
		Model->Samplers.push_back({Wrap(Sampler.wrap_s), Wrap(Sampler.wrap_t), Filter(Sampler.min_filter, false),
		                           Filter(Sampler.mag_filter, true)});
	}
	const auto Binding = [&](const cgltf_texture_view& InView)
	{
		FTextureBinding Result;
		if (!InView.texture)
		{
			return Result;
		}
		Require(InView.texture->image != nullptr, "texture has no supported image fallback");
		Result.Image = static_cast<std::int32_t>(InView.texture->image - Raw->images);
		Result.Sampler =
		    InView.texture->sampler ? static_cast<std::int32_t>(InView.texture->sampler - Raw->samplers) : -1;
		Require(InView.texcoord >= 0 && InView.texcoord <= 1, "only TEXCOORD_0 and TEXCOORD_1 are supported");
		Result.TexCoord = static_cast<std::uint32_t>(InView.texcoord);
		return Result;
	};
	for (std::size_t Index = 0; Index < Raw->materials_count; ++Index)
	{
		const auto& Source = Raw->materials[Index];
		FModelMaterial Material;
		Material.Name = Name(Source.name);
		if (Source.has_pbr_metallic_roughness)
		{
			const auto& Pbr = Source.pbr_metallic_roughness;
			Material.BaseColor = {Pbr.base_color_factor[0], Pbr.base_color_factor[1], Pbr.base_color_factor[2],
			                      Pbr.base_color_factor[3]};
			Material.Metallic = Pbr.metallic_factor;
			Material.Roughness = Pbr.roughness_factor;
			Material.BaseColorTexture = Binding(Pbr.base_color_texture);
			Material.MetallicRoughnessTexture = Binding(Pbr.metallic_roughness_texture);
		}
		Material.Emissive = {Source.emissive_factor[0], Source.emissive_factor[1], Source.emissive_factor[2]};
		Material.NormalTexture = Binding(Source.normal_texture);
		Material.OcclusionTexture = Binding(Source.occlusion_texture);
		Material.EmissiveTexture = Binding(Source.emissive_texture);
		Material.NormalScale = Source.normal_texture.scale;
		Material.OcclusionStrength = Source.occlusion_texture.scale;
		Material.AlphaCutoff = Source.alpha_cutoff;
		Material.AlphaMode = Source.alpha_mode == cgltf_alpha_mode_mask    ? EAlphaMode::Mask
		                     : Source.alpha_mode == cgltf_alpha_mode_blend ? EAlphaMode::Blend
		                                                                   : EAlphaMode::Opaque;
		Material.DoubleSided = Source.double_sided != 0;
		Material.Unlit = Source.unlit != 0;
		Model->Materials.push_back(std::move(Material));
	}
	std::vector<std::vector<std::uint32_t>> MeshPrimitives(Raw->meshes_count);
	for (std::size_t MeshIndex = 0; MeshIndex < Raw->meshes_count; ++MeshIndex)
	{
		const auto& Mesh = Raw->meshes[MeshIndex];
		for (std::size_t PrimitiveIndex = 0; PrimitiveIndex < Mesh.primitives_count; ++PrimitiveIndex)
		{
			InContext.Cancellation.Check();
			const auto& Source = Mesh.primitives[PrimitiveIndex];
			Require(!Source.targets_count, "morph targets are outside the static importer");
			Require(Source.type == cgltf_primitive_type_triangles ||
			            Source.type == cgltf_primitive_type_triangle_strip ||
			            Source.type == cgltf_primitive_type_triangle_fan,
			        "unsupported primitive topology");
			const auto* Positions = cgltf_find_accessor(&Source, cgltf_attribute_type_position, 0);
			Require(Positions && Positions->count && Positions->count <= 8000000, "missing or excessive positions");
			const auto Count = Positions->count;
			FModelPrimitive Primitive;
			Primitive.Name = Name(Mesh.name);
			Primitive.Material = Source.material ? static_cast<std::int32_t>(Source.material - Raw->materials) : -1;
			Primitive.Positions = Attribute(Source, cgltf_attribute_type_position, 0, 3, Count);
			Primitive.Normals = Attribute(Source, cgltf_attribute_type_normal, 0, 3, Count);
			Primitive.Tangents = Attribute(Source, cgltf_attribute_type_tangent, 0, 4, Count);
			Primitive.Colors = Attribute(Source, cgltf_attribute_type_color, 0, 4, Count);
			Primitive.TexCoords0 = Attribute(Source, cgltf_attribute_type_texcoord, 0, 2, Count);
			Primitive.TexCoords1 = Attribute(Source, cgltf_attribute_type_texcoord, 1, 2, Count);
			auto SourceIndices = Indices(Source.indices, Count);
			if (Source.type == cgltf_primitive_type_triangles)
			{
				Require(SourceIndices.size() % 3 == 0, "incomplete triangles");
				Primitive.Indices = std::move(SourceIndices);
			}
			else
			{
				for (std::size_t Index = 2; Index < SourceIndices.size(); ++Index)
				{
					const auto A = Source.type == cgltf_primitive_type_triangle_fan
					                   ? SourceIndices[0]
					                   : SourceIndices[Index - 2 + (Index & 1)];
					const auto B = Source.type == cgltf_primitive_type_triangle_fan
					                   ? SourceIndices[Index - 1]
					                   : SourceIndices[Index - 1 - (Index & 1)];
					Primitive.Indices.insert(Primitive.Indices.end(), {A, B, SourceIndices[Index]});
				}
			}
			// glTF's absent normals imply flat shading, so split shared corners.
			if (Primitive.Normals.empty())
			{
				const auto Expand = [&](std::vector<float>& InValues, std::size_t InComponents)
				{
					if (InValues.empty())
					{
						return;
					}
					Require(Primitive.Indices.size() <= MaxImportBytes / sizeof(float) / InComponents,
					        "expanded geometry exceeds budget");
					std::vector<float> Expanded;
					Expanded.reserve(Primitive.Indices.size() * InComponents);
					for (auto Index : Primitive.Indices)
					{
						Expanded.insert(Expanded.end(), InValues.begin() + Index * InComponents,
						                InValues.begin() + (Index + 1) * InComponents);
					}
					InValues = std::move(Expanded);
				};
				Expand(Primitive.Positions, 3);
				Expand(Primitive.Tangents, 4);
				Expand(Primitive.Colors, 4);
				Expand(Primitive.TexCoords0, 2);
				Expand(Primitive.TexCoords1, 2);
				std::iota(Primitive.Indices.begin(), Primitive.Indices.end(), 0u);
			}
			std::uint32_t TangentUv{};
			if (Primitive.Material >= 0)
			{
				const auto& Material = Model->Materials[Primitive.Material];
				TangentUv = Material.NormalTexture.TexCoord;
				for (auto View : {Material.BaseColorTexture, Material.MetallicRoughnessTexture, Material.NormalTexture,
				                  Material.OcclusionTexture, Material.EmissiveTexture})
				{
					Require(View.Image < 0 || !(View.TexCoord ? Primitive.TexCoords1 : Primitive.TexCoords0).empty(),
					        "material requires a missing UV set");
				}
			}
			GenerateMeshDirections(Primitive, TangentUv);
			const auto GeometryBytes =
			    (Primitive.Positions.size() + Primitive.Normals.size() + Primitive.Tangents.size() +
			     Primitive.Colors.size() + Primitive.TexCoords0.size() + Primitive.TexCoords1.size() +
			     Primitive.Indices.size()) *
			    4;
			Require(GeometryBytes <= MaxImportBytes - Total, "decoded geometry budget exceeded");
			Total += GeometryBytes;
			MeshPrimitives[MeshIndex].push_back(static_cast<std::uint32_t>(Model->Primitives.size()));
			Model->Primitives.push_back(std::move(Primitive));
		}
	}
	for (std::size_t Index = 0; Index < Raw->nodes_count; ++Index)
	{
		const auto& Source = Raw->nodes[Index];
		if (Source.mesh)
		{
			Model->Nodes[Index].Primitives = MeshPrimitives[Source.mesh - Raw->meshes];
		}
	}
	const auto* Scene = Raw->scene ? Raw->scene : Raw->scenes_count ? &Raw->scenes[0] : nullptr;
	if (Scene)
	{
		for (std::size_t Index = 0; Index < Scene->nodes_count; ++Index)
		{
			Model->Roots.push_back(static_cast<std::uint32_t>(Scene->nodes[Index] - Raw->nodes));
		}
	}
	else
	{
		for (std::size_t Index = 0; Index < Raw->nodes_count; ++Index)
		{
			if (!Raw->nodes[Index].parent)
			{
				Model->Roots.push_back(static_cast<std::uint32_t>(Index));
			}
		}
		if (Raw->nodes_count == 0)
		{
			for (const auto& Primitives : MeshPrimitives)
			{
				Model->Roots.push_back(static_cast<std::uint32_t>(Model->Nodes.size()));
				Model->Nodes.push_back({"mesh", Identity(), Primitives, {}});
			}
		}
	}
	ValidateModel(*Model);
	return Model;
}
} // namespace

void RegisterGltfImporter(FAssetService& InAssets)
{
	InAssets.Register({RecordType<FModelAsset>().Id, {".gltf", ".glb"}, Import});
}
} // namespace Hyperion
