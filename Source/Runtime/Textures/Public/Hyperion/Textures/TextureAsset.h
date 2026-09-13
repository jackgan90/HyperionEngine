#pragma once
#include "Hyperion/Reflection/Record.h"
#include <array>

namespace Hyperion
{
enum class EMaterialTextureEncoding : std::uint8_t
{
	Linear = 0,
	Srgb = 1
};

enum class ETextureDimension : std::uint8_t
{
	Texture2D,
	Cube
};

enum class ETextureFormat : std::uint8_t
{
	Rgba8Unorm,
	Rgba16Float,
	Rgba32Float
};

struct FMaterialTextureMip
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	// Each cube mip contains six tightly packed face planes: +X, -X, +Y, -Y, +Z, -Z.
	// RGBA components use little-endian bytes for the owning texture's format.
	std::vector<std::uint8_t> Bytes;
	bool operator==(const FMaterialTextureMip&) const = default;
};

struct FTextureAsset
{
	std::string Name;
	EMaterialTextureEncoding Encoding = EMaterialTextureEncoding::Linear;
	std::vector<FMaterialTextureMip> Mips;
	ETextureDimension Dimension = ETextureDimension::Texture2D;
	ETextureFormat Format = ETextureFormat::Rgba8Unorm;
};

std::uint32_t TexturePixelBytes(ETextureFormat InFormat);
std::array<float, 4> ReadTexturePixel(const FMaterialTextureMip& InMip, ETextureFormat InFormat, std::size_t InPixel);
void WriteTexturePixel(FMaterialTextureMip& InMip, ETextureFormat InFormat, std::size_t InPixel,
                       std::array<float, 4> InValue);
template<> std::span<const ETextureDimension> RecordEnumValues<ETextureDimension>();
template<> std::span<const ETextureFormat> RecordEnumValues<ETextureFormat>();

void ValidateTextureAsset(const FTextureAsset& InAsset);
FTextureAsset BuildTextureAsset(std::string InName, EMaterialTextureEncoding InEncoding, FMaterialTextureMip InBase);
template<> std::span<const EMaterialTextureEncoding> RecordEnumValues<EMaterialTextureEncoding>();
template<> const FRecordDescriptor& RecordType<FMaterialTextureMip>();
template<> const FRecordDescriptor& RecordType<FTextureAsset>();
} // namespace Hyperion
