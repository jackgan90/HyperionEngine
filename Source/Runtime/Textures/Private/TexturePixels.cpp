#include "Hyperion/Textures/TextureAsset.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace Hyperion
{
namespace
{
float DecodeHalf(std::uint32_t InBits)
{
	const auto Exponent = (InBits >> 10) & 31;
	const auto Fraction = InBits & 1023;
	float Result =
	    Exponent ? std::ldexp(1.f + Fraction / 1024.f, int(Exponent) - 15) : std::ldexp(float(Fraction), -24);
	if (Exponent == 31)
	{
		Result = Fraction ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
	}
	return InBits & 32768 ? -Result : Result;
}

std::uint32_t EncodeHalf(float InValue)
{
	if (!std::isfinite(InValue) || std::abs(InValue) > 65504.f)
	{
		throw std::invalid_argument("Pixel exceeds finite RGBA16F range; use RGBA32F");
	}
	const auto Bits = std::bit_cast<std::uint32_t>(InValue);
	const auto Sign = (Bits >> 16) & 32768;
	const int Exponent = int((Bits >> 23) & 255) - 127;
	const auto Mantissa = (Bits & 0x7fffff) | 0x800000;
	if (Exponent < -25)
	{
		return Sign;
	}
	const unsigned Shift = Exponent < -14 ? unsigned(-Exponent - 1) : 13;
	const auto Rounded = (Mantissa + ((1U << (Shift - 1)) - 1) + ((Mantissa >> Shift) & 1)) >> Shift;
	return Sign | (Exponent < -14 ? Rounded : (unsigned(Exponent + 14) << 10) + Rounded);
}
} // namespace

std::uint32_t TexturePixelBytes(ETextureFormat InFormat)
{
	switch (InFormat)
	{
		case ETextureFormat::Rgba8Unorm:
			return 4;
		case ETextureFormat::Rgba16Float:
			return 8;
		case ETextureFormat::Rgba32Float:
			return 16;
	}
	throw std::invalid_argument("Invalid texture format");
}

std::array<float, 4> ReadTexturePixel(const FMaterialTextureMip& InMip, ETextureFormat InFormat, std::size_t InPixel)
{
	const auto PixelBytes = TexturePixelBytes(InFormat);
	if (InPixel >= InMip.Bytes.size() / PixelBytes)
	{
		throw std::out_of_range("Texture pixel");
	}
	const auto* Data = InMip.Bytes.data() + InPixel * PixelBytes;
	std::array<float, 4> Result{};
	for (unsigned Channel = 0; Channel < 4; ++Channel)
	{
		std::uint32_t Bits{};
		for (unsigned Byte = 0; Byte < PixelBytes / 4; ++Byte)
		{
			Bits |= std::uint32_t(Data[Channel * (PixelBytes / 4) + Byte]) << (8 * Byte);
		}
		Result[Channel] = PixelBytes == 4   ? Bits / 255.f
		                  : PixelBytes == 8 ? DecodeHalf(Bits)
		                                    : std::bit_cast<float>(Bits);
	}
	return Result;
}

void WriteTexturePixel(FMaterialTextureMip& InMip, ETextureFormat InFormat, std::size_t InPixel,
                       std::array<float, 4> InValue)
{
	const auto PixelBytes = TexturePixelBytes(InFormat);
	if (InPixel >= InMip.Bytes.size() / PixelBytes)
	{
		throw std::out_of_range("Texture pixel");
	}
	auto* Data = InMip.Bytes.data() + InPixel * PixelBytes;
	for (unsigned Channel = 0; Channel < 4; ++Channel)
	{
		const float Value = InValue[Channel];
		if (!std::isfinite(Value))
		{
			throw std::invalid_argument("Nonfinite texture pixel");
		}
		const std::uint32_t Bits = PixelBytes == 4   ? std::uint32_t(std::lround(std::clamp(Value, 0.f, 1.f) * 255))
		                           : PixelBytes == 8 ? EncodeHalf(Value)
		                                             : std::bit_cast<std::uint32_t>(Value);
		for (unsigned Byte = 0; Byte < PixelBytes / 4; ++Byte)
		{
			Data[Channel * (PixelBytes / 4) + Byte] = std::uint8_t(Bits >> (8 * Byte));
		}
	}
}
} // namespace Hyperion
