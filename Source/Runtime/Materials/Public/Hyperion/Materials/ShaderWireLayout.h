#pragma once
#include "Hyperion/Materials/MaterialBlocks.h"
#include "Hyperion/Math/Math.h"
#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>

namespace Hyperion
{
struct FShaderWireType
{
	EMaterialScalar Scalar = EMaterialScalar::Float;
	std::uint32_t Columns{};
	std::uint32_t Rows{};
	std::size_t Size{};
	bool bSupported{};
};

struct FShaderWireMember
{
	std::string_view Name;
	std::size_t Offset{};
	FShaderWireType Type;
};

// Keep the owning C++ record in the type: descriptors from another record cannot be substituted accidentally.
template<typename T> struct TShaderWireMember
{
	FShaderWireMember Description;
};

template<typename T> constexpr FShaderWireType GetShaderWireType()
{
	using FStorage = std::remove_cv_t<T>;
	constexpr bool bLittleEndian = std::endian::native == std::endian::little;
	if constexpr (std::is_same_v<FStorage, std::uint32_t>)
	{
		return {EMaterialScalar::Uint, 1, 1, sizeof(FStorage),
		        bLittleEndian && sizeof(FStorage) == 4 && std::numeric_limits<unsigned char>::digits == 8};
	}
	else if constexpr (std::is_same_v<FStorage, float>)
	{
		return {EMaterialScalar::Float, 1, 1, sizeof(FStorage),
		        bLittleEndian && sizeof(FStorage) == 4 && std::numeric_limits<float>::is_iec559 &&
		            std::numeric_limits<unsigned char>::digits == 8};
	}
	else if constexpr (std::is_same_v<FStorage, FVec4> && std::is_standard_layout_v<FStorage> &&
	                   std::is_trivially_copyable_v<FStorage>)
	{
		return {EMaterialScalar::Float, 4, 1, sizeof(FStorage),
		        GetShaderWireType<float>().bSupported && std::is_same_v<decltype(FStorage::X), float> &&
		            std::is_same_v<decltype(FStorage::Y), float> && std::is_same_v<decltype(FStorage::Z), float> &&
		            std::is_same_v<decltype(FStorage::W), float> && sizeof(FStorage) == 16 &&
		            offsetof(FStorage, X) == 0 && offsetof(FStorage, Y) == 4 && offsetof(FStorage, Z) == 8 &&
		            offsetof(FStorage, W) == 12};
	}
	else
	{
		// Logical bool, matrix and array mappings deliberately do not prove native wire storage.
		return {};
	}
}

template<typename TRecord, typename TMember>
constexpr TShaderWireMember<TRecord> MakeShaderWireMember(std::string_view InName, std::size_t InOffset)
{
	return {{InName, InOffset, GetShaderWireType<TMember>()}};
}

namespace ShaderWirePrivate
{
std::uint32_t ValidateRecord(const FEngineMaterialResource& InContract, std::size_t InRecordSize, bool bInDirectUpload,
                             std::span<const FShaderWireMember> InMembers);
std::uint32_t ValidateUintPair(const FEngineMaterialResource& InContract, std::size_t InRecordSize,
                               bool bInDirectUpload, const FShaderWireMember& InFirst,
                               const FShaderWireMember& InSecond);
std::uint32_t ValidateScalar(const FEngineMaterialResource& InContract, std::size_t InRecordSize, bool bInDirectUpload,
                             const FShaderWireType& InType);
} // namespace ShaderWirePrivate

template<typename T, std::size_t Count>
std::uint32_t ValidateShaderWireRecord(const FEngineMaterialResource& InContract,
                                       const std::array<TShaderWireMember<T>, Count>& InMembers)
{
	std::array<FShaderWireMember, Count> Members;
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		Members[Index] = InMembers[Index].Description;
	}
	return ShaderWirePrivate::ValidateRecord(InContract, sizeof(T),
	                                         std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>, Members);
}

// Explicitly adapt two actual uint32 fields to one anonymous shader uint2; arbitrary structs are not vectors.
template<typename T>
std::uint32_t ValidateShaderWireUintPair(const FEngineMaterialResource& InContract, const TShaderWireMember<T>& InFirst,
                                         const TShaderWireMember<T>& InSecond)
{
	return ShaderWirePrivate::ValidateUintPair(InContract, sizeof(T),
	                                           std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>,
	                                           InFirst.Description, InSecond.Description);
}

template<typename T> std::uint32_t ValidateShaderWireScalar(const FEngineMaterialResource& InContract)
{
	return ShaderWirePrivate::ValidateScalar(
	    InContract, sizeof(T), std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>, GetShaderWireType<T>());
}
} // namespace Hyperion

// Bind the actual member's type, name and offset to a single token rather than independently authored metadata.
#define HYP_SHADER_WIRE_MEMBER(Record, Member)                                                                         \
	::Hyperion::MakeShaderWireMember<Record, decltype(Record::Member)>(#Member, offsetof(Record, Member))
