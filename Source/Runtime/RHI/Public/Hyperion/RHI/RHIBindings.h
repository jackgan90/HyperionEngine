#pragma once
#include "Hyperion/RHI/RHIResources.h"
#include <array>
#include <cstddef>
#include <cstdint>
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
	RawRead = 16
};

constexpr std::uint32_t BufferUsage(ERHIBufferUsage InUsage)
{
	return static_cast<std::uint32_t>(InUsage);
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

enum class ERHIBindingKind
{
	ConstantBuffer,
	Texture2D,
	StructuredBuffer,
	RawBuffer,
	Sampler
};
enum class ERHIShaderVisibility : std::uint8_t
{
	Vertex = 1,
	Pixel = 2,
	Graphics = 3
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
	bool operator==(const FResourceBindingSlot&) const = default;
};

struct FResourceBindingLayoutDesc
{
	std::vector<FResourceBindingSlot> Slots;
	bool operator==(const FResourceBindingLayoutDesc&) const = default;
};

using FResourceBindingValue = std::variant<FTexture, FReadBufferView, FSampler>;

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
