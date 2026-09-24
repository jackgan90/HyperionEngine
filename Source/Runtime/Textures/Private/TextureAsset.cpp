#include "Hyperion/Textures/TextureAsset.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Hyperion
{
void ValidateTextureAsset(const FTextureAsset& InAsset)
{
	if (InAsset.Mips.empty() || InAsset.Encoding > EMaterialTextureEncoding::Srgb ||
	    InAsset.Dimension > ETextureDimension::Cube || InAsset.Format > ETextureFormat::Rgba32Float ||
	    (InAsset.Format != ETextureFormat::Rgba8Unorm && InAsset.Encoding != EMaterialTextureEncoding::Linear))
	{
		throw std::invalid_argument("Invalid texture encoding, dimension, format or empty mip chain");
	}
	auto Width = InAsset.Mips.front().Width;
	auto Height = InAsset.Mips.front().Height;
	const std::uint32_t Faces = InAsset.Dimension == ETextureDimension::Cube ? 6 : 1;
	if (!Width || !Height || Width > 16384 || Height > 16384 || (Faces == 6 && Width != Height))
	{
		throw std::invalid_argument("Texture dimensions must be between 1 and 16384");
	}
	std::uint64_t Bytes{};
	for (std::size_t Index = 0; Index < InAsset.Mips.size(); ++Index)
	{
		const auto& Mip = InAsset.Mips[Index];
		const auto Extent = std::uint64_t(Width) * Height * Faces * TexturePixelBytes(InAsset.Format);
		Bytes += Extent;
		if (Mip.Width != Width || Mip.Height != Height || Mip.Bytes.size() != Extent || Bytes > 512ULL * 1024 * 1024)
		{
			throw std::invalid_argument("Texture mip dimensions or byte budget mismatch");
		}
		if (InAsset.Format != ETextureFormat::Rgba8Unorm)
		{
			for (std::size_t Pixel = 0; Pixel < std::size_t(Width) * Height * Faces; ++Pixel)
			{
				for (const float Value : ReadTexturePixel(Mip, InAsset.Format, Pixel))
				{
					if (!std::isfinite(Value))
					{
						throw std::invalid_argument("Texture requires finite floating-point pixels");
					}
				}
			}
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

template<> std::span<const TRecordEnumEntry<EMaterialTextureEncoding>> RecordEnumEntries<EMaterialTextureEncoding>()
{
	static constexpr TRecordEnumEntry<EMaterialTextureEncoding> Values[] = {
	    {EMaterialTextureEncoding::Linear, "Linear", ""}, {EMaterialTextureEncoding::Srgb, "Srgb", ""}};
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

template<> std::span<const TRecordEnumEntry<ETextureDimension>> RecordEnumEntries<ETextureDimension>()
{
	static constexpr TRecordEnumEntry<ETextureDimension> Values[] = {{ETextureDimension::Texture2D, "Texture2D", ""},
	                                                                 {ETextureDimension::Cube, "Cube", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<ETextureFormat>> RecordEnumEntries<ETextureFormat>()
{
	static constexpr TRecordEnumEntry<ETextureFormat> Values[] = {{ETextureFormat::Rgba8Unorm, "Rgba8Unorm", ""},
	                                                              {ETextureFormat::Rgba16Float, "Rgba16Float", ""},
	                                                              {ETextureFormat::Rgba32Float, "Rgba32Float", ""}};
	return Values;
}

template<> const FRecordDescriptor& RecordType<FTextureAsset>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FTextureAsset>(
		    "hyperion.textureasset",
		    {Member("name", &FTextureAsset::Name), Member("encoding", &FTextureAsset::Encoding),
		     Member("mips", &FTextureAsset::Mips), Member("dimension", &FTextureAsset::Dimension),
		     Member("format", &FTextureAsset::Format)},
		    2, ValidateTextureAsset);
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		return Result;
	}();
	return Type;
}
} // namespace Hyperion
