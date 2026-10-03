#pragma once
#include <cstdint>
#include <stdexcept>

namespace Hyperion
{
enum class EShaderStage
{
	Vertex,
	Pixel,
	Compute
};

enum class EShaderStageMask : std::uint32_t
{
	None = 0,
	Vertex = 1,
	Pixel = 2,
	Graphics = 3,
	Compute = 4
};

constexpr EShaderStageMask operator|(EShaderStageMask InA, EShaderStageMask InB)
{
	return static_cast<EShaderStageMask>(static_cast<std::uint32_t>(InA) | static_cast<std::uint32_t>(InB));
}

constexpr EShaderStageMask& operator|=(EShaderStageMask& InA, EShaderStageMask InB)
{
	InA = InA | InB;
	return InA;
}

constexpr EShaderStageMask ShaderStageMask(EShaderStage InStage)
{
	switch (InStage)
	{
		case EShaderStage::Vertex:
			return EShaderStageMask::Vertex;
		case EShaderStage::Pixel:
			return EShaderStageMask::Pixel;
		case EShaderStage::Compute:
			return EShaderStageMask::Compute;
	}
	throw std::invalid_argument("Unsupported shader stage");
}

constexpr bool HasAnyShaderStage(EShaderStageMask InStages, EShaderStageMask InOther)
{
	return (static_cast<std::uint32_t>(InStages) & static_cast<std::uint32_t>(InOther)) != 0;
}

constexpr bool HasShaderStage(EShaderStageMask InStages, EShaderStage InStage)
{
	return HasAnyShaderStage(InStages, ShaderStageMask(InStage));
}

constexpr bool IsGraphicsShaderStages(EShaderStageMask InStages)
{
	return InStages == EShaderStageMask::Vertex || InStages == EShaderStageMask::Pixel ||
	       InStages == EShaderStageMask::Graphics;
}
} // namespace Hyperion
