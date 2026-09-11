#pragma once
#include <cstdint>

namespace Hyperion
{
enum class EDepthConvention : std::uint8_t
{
	Standard,
	Reversed
};

constexpr EDepthConvention GetDepthConvention(bool bInReversed)
{
	return bInReversed ? EDepthConvention::Reversed : EDepthConvention::Standard;
}

constexpr float GetDepthClearValue(EDepthConvention InConvention)
{
	return InConvention == EDepthConvention::Reversed ? 0.f : 1.f;
}

// Converts standard depth differences and biases to the selected convention.
constexpr float GetDepthDirection(EDepthConvention InConvention)
{
	return InConvention == EDepthConvention::Reversed ? -1.f : 1.f;
}
} // namespace Hyperion
