#include "AssetTextureDisplay.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
float DisplayChannel(float InLinear)
{
	const float Value = std::clamp(InLinear, 0.f, 1.f);
	return Value <= .0031308f ? Value * 12.92f : 1.055f * std::pow(Value, 1.f / 2.4f) - .055f;
}
} // namespace

FTextureAsset BuildAssetTextureDisplay(const FTextureAsset& InSource, std::size_t InMip, std::size_t InFace,
                                       EAssetPreviewChannel InChannel, float InExposure, bool bInChecker,
                                       FCancellationToken InCancellation)
{
	const auto SelectedComponent = DescribeAssetPreviewChannel(InChannel).Component;
	const auto& Mip = InSource.Mips.at(InMip);
	FMaterialTextureMip Output{Mip.Width, Mip.Height};
	Output.Bytes.resize(std::size_t(Mip.Width) * Mip.Height * 4);
	for (std::uint32_t Y = 0; Y < Mip.Height; ++Y)
	{
		InCancellation.Check();
		for (std::uint32_t X = 0; X < Mip.Width; ++X)
		{
			const auto Pixel = std::size_t(Y) * Mip.Width + X;
			auto Value = ReadTexturePixel(Mip, InSource.Format, InFace * std::size_t(Mip.Width) * Mip.Height + Pixel);
			const float Background = bInChecker ? (((X / 16 + Y / 16) & 1) ? .32f : .18f) : 0.f;
			if (SelectedComponent)
			{
				const float Component = std::clamp(Value.at(*SelectedComponent), 0.f, 1.f);
				Value = {Component, Component, Component, 1};
			}
			else
			{
				for (std::size_t C = 0; C < 3; ++C)
				{
					if (InSource.Encoding == EMaterialTextureEncoding::Linear)
					{
						Value[C] = DisplayChannel(Value[C] * std::exp2(InExposure));
					}
					Value[C] = std::clamp(Value[C], 0.f, 1.f) * std::clamp(Value[3], 0.f, 1.f) +
					           Background * (1 - std::clamp(Value[3], 0.f, 1.f));
				}
				Value[3] = 1;
			}
			WriteTexturePixel(Output, ETextureFormat::Rgba8Unorm, Pixel, Value);
		}
	}
	return BuildTextureAsset("Texture preview", EMaterialTextureEncoding::Srgb, std::move(Output));
}
} // namespace Hyperion
