#include "GltfImportInternal.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <cstring>
#include <numeric>

namespace Hyperion::Private
{
void Require(bool bInValue, const std::string& InMessage)
{
	if (!bInValue)
	{
		throw std::runtime_error("glTF: " + InMessage);
	}
}

std::string Name(const char* InName)
{
	return InName ? InName : "";
}

static FBytes Base64(std::string_view InText)
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

static std::uint32_t Unsigned(const std::uint8_t* InData, cgltf_component_type InType);

static void ValidateRange(const cgltf_buffer_view* InView, std::size_t InOffset, std::size_t InCount,
                          std::size_t InStride, std::size_t InElement)
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

static std::uint32_t Unsigned(const std::uint8_t* InData, cgltf_component_type InType)
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
} // namespace Hyperion::Private
