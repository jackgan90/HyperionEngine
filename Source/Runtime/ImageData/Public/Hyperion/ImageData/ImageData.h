#pragma once
#include <cstdint>
#include <vector>

namespace Hyperion
{
enum class EColorSpace
{
	Linear,
	Srgb
};

// Owned, tightly packed RGBA values. Encoding describes the stored values;
// transferring an image does not implicitly convert its color space.
struct FImage
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	EColorSpace Encoding = EColorSpace::Linear;
	std::vector<float> Rgba;
};

struct FImagePixels
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::vector<std::uint8_t> Rgba;
};
} // namespace Hyperion
