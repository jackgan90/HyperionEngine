#include "Hyperion/Textures/TextureAsset.h"
#include <algorithm>
#include <array>

namespace Hyperion
{
void ValidateTextureAsset(const FTextureAsset& InAsset)
{
	if (InAsset.Mips.empty() || InAsset.Encoding > EMaterialTextureEncoding::Srgb)
	{
		throw std::invalid_argument("Texture requires an encoding and a complete RGBA8 mip chain");
	}
	auto Width = InAsset.Mips.front().Width;
	auto Height = InAsset.Mips.front().Height;
	if (!Width || !Height || Width > 16384 || Height > 16384)
	{
		throw std::invalid_argument("Texture dimensions must be between 1 and 16384");
	}
	std::uint64_t Bytes{};
	for (std::size_t Index = 0; Index < InAsset.Mips.size(); ++Index)
	{
		const auto& Mip = InAsset.Mips[Index];
		const auto Extent = std::uint64_t(Width) * Height * 4;
		Bytes += Extent;
		if (Mip.Width != Width || Mip.Height != Height || Mip.Bytes.size() != Extent || Bytes > 512ULL * 1024 * 1024)
		{
			throw std::invalid_argument("Texture mip dimensions or byte budget mismatch");
		}
		if (Width == 1 && Height == 1 && Index + 1 != InAsset.Mips.size())
		{
			throw std::invalid_argument("Texture mip chain extends beyond 1x1");
		}
		Width = std::max(1U, Width / 2);
		Height = std::max(1U, Height / 2);
	}
	if (InAsset.Mips.back().Width != 1 || InAsset.Mips.back().Height != 1)
	{
		throw std::invalid_argument("Native texture requires a complete mip chain");
	}
}

template<> std::span<const EMaterialTextureEncoding> RecordEnumValues<EMaterialTextureEncoding>()
{
	static constexpr std::array Values{EMaterialTextureEncoding::Linear, EMaterialTextureEncoding::Srgb};
	return Values;
}

template<> const FRecordDescriptor& RecordType<FMaterialTextureMip>()
{
	static const auto Type =
	    MakeRecord<FMaterialTextureMip>("hyperion.texturemip", {Member("width", &FMaterialTextureMip::Width),
	                                                            Member("height", &FMaterialTextureMip::Height),
	                                                            Member("bytes", &FMaterialTextureMip::Bytes)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FTextureAsset>()
{
	static const auto Type =
	    MakeRecord<FTextureAsset>("hyperion.textureasset",
	                              {Member("name", &FTextureAsset::Name), Member("encoding", &FTextureAsset::Encoding),
	                               Member("mips", &FTextureAsset::Mips)},
	                              1, ValidateTextureAsset);
	return Type;
}
} // namespace Hyperion
