#include "Hyperion/Materials/MaterialResources.h"
#include "MaterialIdentity.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Hyperion
{
FMaterialTextureSource::FMaterialTextureSource(std::shared_ptr<const FTextureAsset> InAsset, std::uint64_t InVersion)
    : Identity(MaterialsPrivate::NextIdentity()), Version(InVersion), Encoding(EMaterialTextureEncoding::Linear),
      Asset(std::move(InAsset))
{
	if (!Asset || !Version)
	{
		throw std::invalid_argument("Texture source requires an asset and a nonzero version");
	}
	ValidateTextureAsset(*Asset);
	Encoding = Asset->Encoding;
}

FMaterialTextureSource::FMaterialTextureSource(FMaterialDepthTexture InDepth, std::uint64_t InVersion)
    : Identity(MaterialsPrivate::NextIdentity()), Version(InVersion), Encoding(EMaterialTextureEncoding::Linear),
      Depth(InDepth), bDepthTarget(true)
{
	if (!Version || !Depth.Width || !Depth.Height || Depth.Width > 16384 || Depth.Height > 16384 ||
	    !std::isfinite(Depth.ClearDepth) || Depth.ClearDepth < 0 || Depth.ClearDepth > 1)
	{
		throw std::invalid_argument("Invalid material depth target description");
	}
}

FMaterialTextureSource::FMaterialTextureSource(FMaterialColorTexture InColor, std::uint64_t InVersion)
    : Identity(MaterialsPrivate::NextIdentity()), Version(InVersion), Encoding(EMaterialTextureEncoding::Linear),
      Color(InColor), bColorTarget(true)
{
	if (!Version || !Color.Width || !Color.Height || Color.Width > 16384 || Color.Height > 16384 ||
	    Color.Format > EMaterialColorFormat::Rgba32Float ||
	    !std::all_of(Color.Clear.begin(), Color.Clear.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }))
	{
		throw std::invalid_argument("Invalid material color target description");
	}
}

const FMaterialColorTexture* FMaterialTextureSource::GetColorTarget() const
{
	return bColorTarget ? &Color : nullptr;
}

bool FMaterialTextureSource::IsRenderTarget() const
{
	return bDepthTarget || bColorTarget;
}

const FMaterialDepthTexture* FMaterialTextureSource::GetDepthTarget() const
{
	return bDepthTarget ? &Depth : nullptr;
}

FMaterialTextureSource::FMaterialTextureSource(EMaterialTextureEncoding InEncoding,
                                               std::vector<FMaterialTextureMip> InMips, std::uint64_t InVersion)
    : Identity(MaterialsPrivate::NextIdentity()), Version(InVersion), Encoding(InEncoding), Mips(std::move(InMips))
{
	if (Version == 0 || Mips.empty() ||
	    (Encoding != EMaterialTextureEncoding::Linear && Encoding != EMaterialTextureEncoding::Srgb))
	{
		throw std::invalid_argument("Invalid material texture version, encoding or empty mip chain");
	}
	std::uint32_t Width = Mips.front().Width;
	std::uint32_t Height = Mips.front().Height;
	for (std::size_t Index = 0; Index < Mips.size(); ++Index)
	{
		const FMaterialTextureMip& Mip = Mips[Index];
		const std::uint64_t Pixels = std::uint64_t(Width) * Height;
		if (Width == 0 || Height == 0 || Mip.Width != Width || Mip.Height != Height ||
		    Pixels > std::numeric_limits<std::size_t>::max() / 4 || Mip.Bytes.size() != Pixels * 4)
		{
			throw std::invalid_argument("Invalid material RGBA8 mip dimensions or byte extent");
		}
		if (Width == 1 && Height == 1 && Index + 1 != Mips.size())
		{
			throw std::invalid_argument("Material mip chain extends beyond 1x1");
		}
		Width = std::max(1U, Width / 2);
		Height = std::max(1U, Height / 2);
	}
}

std::uint64_t FMaterialTextureSource::GetIdentity() const
{
	return Identity;
}

std::uint64_t FMaterialTextureSource::GetVersion() const
{
	return Version;
}

EMaterialTextureEncoding FMaterialTextureSource::GetEncoding() const
{
	return Encoding;
}

const std::vector<FMaterialTextureMip>& FMaterialTextureSource::GetMips() const
{
	return Asset ? Asset->Mips : Mips;
}

const std::shared_ptr<const FTextureAsset>& FMaterialTextureSource::GetAsset() const
{
	return Asset;
}

FMaterialReadBufferSource::FMaterialReadBufferSource(std::span<const std::byte> InBytes, std::uint64_t InVersion)
    : Identity(MaterialsPrivate::NextIdentity()), Version(InVersion), Bytes(InBytes.begin(), InBytes.end())
{
	if (Version == 0 || Bytes.empty())
	{
		throw std::invalid_argument("Material read buffer requires bytes and a nonzero version");
	}
}

std::uint64_t FMaterialReadBufferSource::GetIdentity() const
{
	return Identity;
}

std::uint64_t FMaterialReadBufferSource::GetVersion() const
{
	return Version;
}

std::span<const std::byte> FMaterialReadBufferSource::GetBytes() const
{
	return Bytes;
}

void FMaterialBufferView::Validate() const
{
	if (!Source || Size == 0 || Offset > Source->GetBytes().size() || Size > Source->GetBytes().size() - Offset)
	{
		throw std::invalid_argument("Material read buffer view exceeds its owned source");
	}
	if (Kind == EMaterialBufferViewKind::Raw)
	{
		if (Stride != 0 || Offset % 4 != 0 || Size % 4 != 0)
		{
			throw std::invalid_argument("Raw material buffer views require four-byte ranges and zero stride");
		}
	}
	else if (Kind == EMaterialBufferViewKind::Structured)
	{
		if (Stride == 0 || Stride > 2048 || Stride % 4 != 0 || Offset % Stride != 0 || Size % Stride != 0)
		{
			throw std::invalid_argument("Invalid material structured buffer stride or range");
		}
	}
	else
	{
		throw std::invalid_argument("Unsupported material buffer view kind");
	}
}

void FMaterialSampler::Validate() const
{
	if (U > EMaterialAddressMode::MirrorOnce || V > EMaterialAddressMode::MirrorOnce ||
	    W > EMaterialAddressMode::MirrorOnce || Compare > EMaterialSamplerCompare::Always || MaxAnisotropy < 1 ||
	    MaxAnisotropy > 16 || !std::isfinite(MipLodBias) ||
	    (MaxAnisotropy > 1 && (!bMinLinear || !bMagLinear || !bMipLinear)) || !std::isfinite(MinLod) ||
	    !std::isfinite(MaxLod) || MinLod > MaxLod || MipLodBias < -16 || MipLodBias > 15.99F ||
	    !std::all_of(BorderColor.begin(), BorderColor.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }))
	{
		throw std::invalid_argument("Invalid material sampler address, filter or LOD values");
	}
}
} // namespace Hyperion
