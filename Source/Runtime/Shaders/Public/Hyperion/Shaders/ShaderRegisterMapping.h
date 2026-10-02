#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace Hyperion
{
enum class EShaderRegisterClass
{
	ConstantBuffer,
	ShaderResource,
	Sampler,
	UnorderedAccess
};

inline constexpr std::uint32_t ShaderBindingMappingVersion = 2;
inline constexpr std::uint32_t ShaderRegisterSpaceCount = 4;
inline constexpr std::uint32_t ShaderRegistersPerKind = 1000;

struct FShaderRegisterClassMapping
{
	EShaderRegisterClass Class;
	std::uint32_t BindingOffset;
};

// These offsets are the binding protocol, independent of resource-kind enum ordinals.
inline constexpr std::array ShaderRegisterClassMappings{
    FShaderRegisterClassMapping{EShaderRegisterClass::ConstantBuffer, 0},
    FShaderRegisterClassMapping{EShaderRegisterClass::ShaderResource, 1000},
    FShaderRegisterClassMapping{EShaderRegisterClass::Sampler, 2000},
    FShaderRegisterClassMapping{EShaderRegisterClass::UnorderedAccess, 3000}};

struct FShaderRegisterAddress
{
	EShaderRegisterClass Class;
	std::uint32_t Space;
	std::uint32_t Register;
	bool operator==(const FShaderRegisterAddress&) const = default;
};

constexpr bool IsValidShaderRegisterRange(std::uint32_t InSpace, std::uint32_t InRegister, std::uint32_t InCount = 1)
{
	return InSpace < ShaderRegisterSpaceCount && InRegister < ShaderRegistersPerKind && InCount != 0 &&
	       InCount <= ShaderRegistersPerKind - InRegister;
}

constexpr std::optional<std::uint32_t> EncodeShaderBinding(EShaderRegisterClass InClass, std::uint32_t InSpace,
                                                           std::uint32_t InRegister, std::uint32_t InCount = 1)
{
	if (!IsValidShaderRegisterRange(InSpace, InRegister, InCount))
	{
		return std::nullopt;
	}
	for (const auto& Mapping : ShaderRegisterClassMappings)
	{
		if (Mapping.Class == InClass)
		{
			return Mapping.BindingOffset + InRegister;
		}
	}
	return std::nullopt;
}

constexpr std::optional<FShaderRegisterAddress> DecodeShaderBinding(std::uint32_t InSpace, std::uint32_t InBinding,
                                                                    std::uint32_t InCount = 1)
{
	for (const auto& Mapping : ShaderRegisterClassMappings)
	{
		if (InBinding >= Mapping.BindingOffset &&
		    IsValidShaderRegisterRange(InSpace, InBinding - Mapping.BindingOffset, InCount))
		{
			return FShaderRegisterAddress{Mapping.Class, InSpace, InBinding - Mapping.BindingOffset};
		}
	}
	return std::nullopt;
}
} // namespace Hyperion
