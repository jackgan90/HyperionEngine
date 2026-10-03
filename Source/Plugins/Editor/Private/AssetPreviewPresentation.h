#pragma once
#include "Hyperion/Textures/TextureAsset.h"
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Hyperion
{
// GUI positions and captions are presentation only; the selected row carries the domain identity.
template<class TOption, class TIdentity>
std::size_t AssetPreviewOptionIndex(std::span<const TOption> InOptions, TIdentity InId)
{
	for (std::size_t Index = 0; Index < InOptions.size(); ++Index)
	{
		if (InOptions[Index].Id == InId)
		{
			return Index;
		}
	}
	throw std::invalid_argument("Preview option identity is absent from the presentation");
}

template<class TOption> auto AssetPreviewOptionIdentity(std::span<const TOption> InOptions, std::size_t InIndex)
{
	if (InIndex >= InOptions.size())
	{
		throw std::invalid_argument("Preview option presentation index is out of range");
	}
	return InOptions[InIndex].Id;
}

template<class TOption> std::vector<std::string> AssetPreviewOptionLabels(std::span<const TOption> InOptions)
{
	std::vector<std::string> Result;
	for (const auto& Option : InOptions)
	{
		Result.emplace_back(Option.Label);
	}
	return Result;
}

struct FAssetPreviewFormatDescription
{
	std::string_view Label;
	std::string_view CompactLabel;
};

inline FAssetPreviewFormatDescription DescribeAssetPreviewFormat(ETextureFormat InFormat)
{
	switch (InFormat)
	{
		case ETextureFormat::Rgba8Unorm:
			return {"RGBA8 UNORM", "RGBA8"};
		case ETextureFormat::Rgba16Float:
			return {"RGBA16 FLOAT", "RGBA16F"};
		case ETextureFormat::Rgba32Float:
			return {"RGBA32 FLOAT", "RGBA32F"};
	}
	throw std::invalid_argument("Unsupported asset preview texture format");
}
} // namespace Hyperion
