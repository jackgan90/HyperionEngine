#pragma once
#include "Hyperion/Reflection/Record.h"

namespace Hyperion
{
enum class EMaterialTextureEncoding : std::uint8_t
{
	Linear = 0,
	Srgb = 1
};

struct FMaterialTextureMip
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::vector<std::uint8_t> Bytes;
	bool operator==(const FMaterialTextureMip&) const = default;
};

struct FTextureAsset
{
	std::string Name;
	EMaterialTextureEncoding Encoding = EMaterialTextureEncoding::Linear;
	std::vector<FMaterialTextureMip> Mips;
};

void ValidateTextureAsset(const FTextureAsset& InAsset);
FTextureAsset BuildTextureAsset(std::string InName, EMaterialTextureEncoding InEncoding, FMaterialTextureMip InBase);
template<> std::span<const EMaterialTextureEncoding> RecordEnumValues<EMaterialTextureEncoding>();
template<> const FRecordDescriptor& RecordType<FMaterialTextureMip>();
template<> const FRecordDescriptor& RecordType<FTextureAsset>();
} // namespace Hyperion
