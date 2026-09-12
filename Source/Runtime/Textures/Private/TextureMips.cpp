#include "Hyperion/Textures/TextureAsset.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
float Linear(float InValue)
{
	return InValue <= .04045f ? InValue / 12.92f : std::pow((InValue + .055f) / 1.055f, 2.4f);
}

float Srgb(float InValue)
{
	return InValue <= .0031308f ? InValue * 12.92f : 1.055f * std::pow(InValue, 1 / 2.4f) - .055f;
}

FMaterialTextureMip Downsample(const FMaterialTextureMip& InPrevious, bool bInSrgb)
{
	FMaterialTextureMip Mip;
	Mip.Width = std::max(1U, InPrevious.Width / 2);
	Mip.Height = std::max(1U, InPrevious.Height / 2);
	Mip.Bytes.resize(std::size_t(Mip.Width) * Mip.Height * 4);
	for (std::uint32_t Y = 0; Y < Mip.Height; ++Y)
	{
		for (std::uint32_t X = 0; X < Mip.Width; ++X)
		{
			for (std::uint32_t Channel = 0; Channel < 4; ++Channel)
			{
				float Sum{};
				unsigned Count{};
				for (auto Row = Y * InPrevious.Height / Mip.Height; Row < (Y + 1) * InPrevious.Height / Mip.Height;
				     ++Row)
				{
					for (auto Column = X * InPrevious.Width / Mip.Width;
					     Column < (X + 1) * InPrevious.Width / Mip.Width; ++Column)
					{
						const float Value =
						    InPrevious.Bytes[(std::size_t(Row) * InPrevious.Width + Column) * 4 + Channel] / 255.f;
						Sum += bInSrgb && Channel < 3 ? Linear(Value) : Value;
						++Count;
					}
				}
				const float Average = Sum / Count;
				const float Value = bInSrgb && Channel < 3 ? Srgb(Average) : Average;
				Mip.Bytes[(std::size_t(Y) * Mip.Width + X) * 4 + Channel] =
				    static_cast<std::uint8_t>(std::lround(std::clamp(Value, 0.f, 1.f) * 255));
			}
		}
	}
	return Mip;
}
} // namespace

FTextureAsset BuildTextureAsset(std::string InName, EMaterialTextureEncoding InEncoding, FMaterialTextureMip InBase)
{
	if (!InBase.Width || !InBase.Height || InBase.Width > 16384 || InBase.Height > 16384 ||
	    InBase.Bytes.size() != std::uint64_t(InBase.Width) * InBase.Height * 4 ||
	    InBase.Bytes.size() > 384ULL * 1024 * 1024)
	{
		throw std::invalid_argument("Invalid texture base level or mip generation budget");
	}
	FTextureAsset Result;
	Result.Name = std::move(InName);
	Result.Encoding = InEncoding;
	Result.Mips.push_back(std::move(InBase));
	while (Result.Mips.back().Width > 1 || Result.Mips.back().Height > 1)
	{
		Result.Mips.push_back(Downsample(Result.Mips.back(), InEncoding == EMaterialTextureEncoding::Srgb));
	}
	ValidateTextureAsset(Result);
	return Result;
}
} // namespace Hyperion
