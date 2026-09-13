#pragma once
#include "Hyperion/Environment/SkyAsset.h"
#include <cmath>
#include <numbers>

namespace Hyperion::EnvironmentPrivate
{
inline FVec3 GgxHalf(unsigned InIndex, unsigned InCount, float InRoughness)
{
	unsigned Bits = InIndex;
	Bits = (Bits << 16) | (Bits >> 16);
	Bits = ((Bits & 0x55555555U) << 1) | ((Bits & 0xaaaaaaaaU) >> 1);
	Bits = ((Bits & 0x33333333U) << 2) | ((Bits & 0xccccccccU) >> 2);
	Bits = ((Bits & 0x0f0f0f0fU) << 4) | ((Bits & 0xf0f0f0f0U) >> 4);
	Bits = ((Bits & 0x00ff00ffU) << 8) | ((Bits & 0xff00ff00U) >> 8);
	const float Xi = Bits * 2.3283064365386963e-10f;
	const float Phi = 2 * std::numbers::pi_v<float> * InIndex / InCount;
	const float A = InRoughness * InRoughness;
	const float Cosine = std::sqrt((1 - Xi) / (1 + (A * A - 1) * Xi));
	const float Sine = std::sqrt(std::max(0.f, 1 - Cosine * Cosine));
	return {Sine * std::cos(Phi), Sine * std::sin(Phi), Cosine};
}

inline FMaterialTextureMip MakeMip(unsigned InSize, unsigned InFaces, ETextureFormat InFormat)
{
	return {InSize, InSize,
	        std::vector<std::uint8_t>(std::size_t(InSize) * InSize * InFaces * TexturePixelBytes(InFormat))};
}
} // namespace Hyperion::EnvironmentPrivate
