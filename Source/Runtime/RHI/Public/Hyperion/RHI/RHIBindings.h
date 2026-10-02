#pragma once
#include "Hyperion/RHI/RHIResources.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <variant>
#include <vector>

namespace Hyperion
{
enum class ERHIBufferUsage : std::uint32_t
{
	Vertex = 1,
	Index = 2,
	Constant = 4,
	StructuredRead = 8,
	RawRead = 16,
	StructuredWrite = 32,
	RawWrite = 64
};

constexpr std::uint32_t BufferUsage(ERHIBufferUsage InUsage)
{
	return static_cast<std::uint32_t>(InUsage);
}

constexpr std::uint32_t BufferUsage(std::initializer_list<ERHIBufferUsage> InUsages)
{
	std::uint32_t Result{};
	for (const auto Usage : InUsages)
	{
		Result |= BufferUsage(Usage);
	}
	return Result;
}

inline constexpr std::uint32_t ShaderReadBufferUsages =
    BufferUsage({ERHIBufferUsage::StructuredRead, ERHIBufferUsage::RawRead});
inline constexpr std::uint32_t ShaderWriteBufferUsages =
    BufferUsage({ERHIBufferUsage::StructuredWrite, ERHIBufferUsage::RawWrite});
inline constexpr std::uint32_t KnownBufferUsages =
    BufferUsage({ERHIBufferUsage::Vertex, ERHIBufferUsage::Index, ERHIBufferUsage::Constant}) | ShaderReadBufferUsages |
    ShaderWriteBufferUsages;

constexpr bool HasAnyBufferUsage(std::uint32_t InUsage, std::uint32_t InUsages)
{
	return (InUsage & InUsages) != 0;
}

constexpr bool HasOnlyBufferUsage(std::uint32_t InUsage, std::uint32_t InAllowedUsages)
{
	return (InUsage & ~InAllowedUsages) == 0;
}

// Membership only; graph support and native allocation/access restrictions remain with their owners.
constexpr bool IsKnownBufferUsage(std::uint32_t InUsage)
{
	return InUsage != 0 && HasOnlyBufferUsage(InUsage, KnownBufferUsages);
}

struct FBufferDesc
{
	std::uint64_t Size{};
	std::uint32_t Usage = BufferUsage(ERHIBufferUsage::Vertex) | BufferUsage(ERHIBufferUsage::Index);
};

struct FBufferSlice
{
	FBuffer Buffer;
	std::uint64_t Offset{};
	std::uint32_t Size{};
	std::uint32_t Extent{};
	std::uint64_t Publication{};
	bool operator==(const FBufferSlice&) const = default;
};

enum class ERHIBufferViewKind
{
	Structured,
	Raw
};

struct FReadBufferView
{
	FBuffer Buffer;
	ERHIBufferViewKind Kind = ERHIBufferViewKind::Raw;
	std::uint64_t Offset{};
	std::uint64_t Size{};
	std::uint32_t Stride{};
	bool operator==(const FReadBufferView&) const = default;
};

struct FTextureView
{
	FTexture Texture;
	std::uint32_t FirstMip{};
	std::uint32_t MipCount = 1;
	bool operator==(const FTextureView&) const = default;
};

enum class ERHIBindingKind
{
	ConstantBuffer,
	Texture2D,
	StructuredBuffer,
	RawBuffer,
	Sampler,
	TextureCube,
	StorageTexture2D,
	StorageStructuredBuffer,
	StorageRawBuffer
};

constexpr bool IsStorageBinding(ERHIBindingKind InKind)
{
	return InKind == ERHIBindingKind::StorageTexture2D || InKind == ERHIBindingKind::StorageStructuredBuffer ||
	       InKind == ERHIBindingKind::StorageRawBuffer;
}
enum class ERHIShaderVisibility : std::uint8_t
{
	Vertex = 1,
	Pixel = 2,
	Graphics = 3,
	Compute = 4
};

struct FResourceBindingSlot
{
	ERHIBindingKind Kind = ERHIBindingKind::ConstantBuffer;
	ERHIShaderVisibility Visibility = ERHIShaderVisibility::Graphics;
	std::uint32_t Register{};
	std::uint32_t Space{};
	std::uint32_t Count = 1;
	std::uint32_t MinimumBufferSize{};
	std::uint32_t StructureByteStride{}; // Zero is unconstrained until a structured shader requires an exact stride.
	std::uint32_t InstanceStride{}; // Nonzero: explicitly declared constant array indexed by the local instance ID.
	std::uint32_t InstanceCapacity{};
	bool bComparison{}; // Sampler kind is part of the reflected layout contract.
	bool operator==(const FResourceBindingSlot&) const = default;
};

struct FResourceBindingLayoutDesc
{
	std::vector<FResourceBindingSlot> Slots;
	bool operator==(const FResourceBindingLayoutDesc&) const = default;
};

using FResourceBindingValue = std::variant<FTexture, FReadBufferView, FSampler, FTextureView>;

struct FResourceBindingEntry
{
	std::uint32_t Slot{};
	std::vector<FResourceBindingValue> Values;
	bool operator==(const FResourceBindingEntry&) const = default;
};

struct FResourceBindingSetDesc
{
	FResourceBindingLayout Layout;
	std::vector<FResourceBindingEntry> Entries;
	bool operator==(const FResourceBindingSetDesc&) const = default;
};

struct FConstantBinding
{
	std::uint32_t Slot{};
	FBufferSlice Slice;
};
} // namespace Hyperion
