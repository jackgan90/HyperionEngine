#pragma once
#include "Hyperion/Environment/SkyAsset.h"
#include <array>
#include <memory>
#include <span>
#include <string_view>

namespace Hyperion
{
struct FSkyPreviewProducts
{
	std::shared_ptr<const FTextureAsset> Radiance;
	std::shared_ptr<const FTextureAsset> Specular;
	std::shared_ptr<const FTextureAsset> Brdf;
};

struct FSkyPreviewProductDescription
{
	std::string_view Label;
	FAssetRef FSkyAsset::* Reference;
	std::shared_ptr<const FTextureAsset> FSkyPreviewProducts::* Texture;
};

inline std::span<const FSkyPreviewProductDescription> SkyPreviewProductDescriptions()
{
	static constexpr std::array Products{
	    FSkyPreviewProductDescription{"Radiance", &FSkyAsset::Radiance, &FSkyPreviewProducts::Radiance},
	    FSkyPreviewProductDescription{"Specular", &FSkyAsset::Specular, &FSkyPreviewProducts::Specular},
	    FSkyPreviewProductDescription{"BRDF", &FSkyAsset::Brdf, &FSkyPreviewProducts::Brdf}};
	return Products;
}
} // namespace Hyperion
