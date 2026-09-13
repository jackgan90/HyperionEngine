#include "EnvironmentSampling.h"
#include <algorithm>
#include <bit>

namespace Hyperion
{
namespace
{
struct FPanorama
{
	std::span<const float> Rgba;
	unsigned Width{};
	unsigned Height{};

	FVec3 Sample(FVec3 InDirection) const
	{
		const float X = (std::atan2(InDirection.Z, InDirection.X) / (2 * std::numbers::pi_v<float>)+.5f) * Width - .5f;
		const float Y = std::acos(std::clamp(InDirection.Y, -1.f, 1.f)) / std::numbers::pi_v<float> * Height - .5f;
		const int Left = int(std::floor(X));
		const int Top = int(std::floor(Y));
		FVec3 Result;
		for (int Row = 0; Row < 2; ++Row)
		{
			for (int Column = 0; Column < 2; ++Column)
			{
				const auto U = (Left + Column + int(Width)) % int(Width);
				const auto V = std::clamp(Top + Row, 0, int(Height) - 1);
				const auto Index = (std::size_t(V) * Width + U) * 4;
				const float Weight = (Column ? X - Left : 1 - X + Left) * (Row ? Y - Top : 1 - Y + Top);
				Result = Add(Result, ScaleVector({Rgba[Index], Rgba[Index + 1], Rgba[Index + 2]}, Weight));
			}
		}
		return Result;
	}
};

void Write(FMaterialTextureMip& InMip, ETextureFormat InFormat, unsigned InFace, unsigned InX, unsigned InY,
           FVec3 InValue)
{
	WriteTexturePixel(InMip, InFormat, (std::size_t(InFace) * InMip.Height + InY) * InMip.Width + InX,
	                  {InValue.X, InValue.Y, InValue.Z, 1});
}

FTextureAsset ConvertPanorama(const FPanorama& InImage, unsigned InSize, ETextureFormat InFormat)
{
	FTextureAsset Result;
	Result.Name = "Sky radiance";
	Result.Dimension = ETextureDimension::Cube;
	Result.Format = InFormat;
	for (auto Size = InSize;; Size /= 2)
	{
		auto Mip = EnvironmentPrivate::MakeMip(Size, 6, InFormat);
		for (unsigned Face = 0; Face < 6; ++Face)
		{
			for (unsigned Y = 0; Y < Size; ++Y)
			{
				for (unsigned X = 0; X < Size; ++X)
				{
					FVec3 Value;
					for (unsigned Sample = 0; Sample < 4; ++Sample)
					{
						const auto Direction = CubeDirection(Face, (X + .25f + .5f * (Sample % 2)) * 2 / Size - 1,
						                                     (Y + .25f + .5f * (Sample / 2)) * 2 / Size - 1);
						Value = Add(Value,
						            ScaleVector(Result.Mips.empty() ? InImage.Sample(Direction)
						                                            : SampleEnvironment(Result, Direction,
						                                                                float(Result.Mips.size() - 1)),
						                        .25f));
					}
					Write(Mip, InFormat, Face, X, Y, Value);
				}
			}
		}
		Result.Mips.push_back(std::move(Mip));
		if (Size == 1)
		{
			break;
		}
	}
	return Result;
}

struct FFilterSample
{
	FVec3 Direction;
	float Mip{};
};

std::vector<FFilterSample> FilterSamples(float InRoughness, unsigned InCount, unsigned InSourceSize)
{
	std::vector<FFilterSample> Result;
	const float A = InRoughness * InRoughness;
	const float TexelArea = 4 * std::numbers::pi_v<float> / (6.f * InSourceSize * InSourceSize);
	for (unsigned Index = 0; Index < InCount; ++Index)
	{
		const auto H = EnvironmentPrivate::GgxHalf(Index, InCount, InRoughness);
		const auto L = Subtract(ScaleVector(H, 2 * H.Z), {0, 0, 1});
		if (L.Z > 0)
		{
			const float Denominator = H.Z * H.Z * (A * A - 1) + 1;
			const float Pdf = A * A / std::max(4 * std::numbers::pi_v<float> * Denominator * Denominator, 1e-12f);
			const float Mip = .5f * std::log2(1.f / std::max(InCount * Pdf * TexelArea, 1e-12f));
			Result.push_back({L, std::max(0.f, Mip)});
		}
	}
	return Result;
}

FVec3 Filter(const FTextureAsset& InCube, FVec3 InNormal, std::span<const FFilterSample> InSamples)
{
	const auto Tangent = Normalize(Cross(std::abs(InNormal.Z) < .999f ? FVec3{0, 0, 1} : FVec3{1, 0, 0}, InNormal));
	const auto Bitangent = Cross(InNormal, Tangent);
	FVec3 Sum;
	float Weight{};
	for (const auto& Sample : InSamples)
	{
		const auto& L = Sample.Direction;
		const auto Direction =
		    Add(Add(ScaleVector(Tangent, L.X), ScaleVector(Bitangent, L.Y)), ScaleVector(InNormal, L.Z));
		Sum = Add(Sum, ScaleVector(SampleEnvironment(InCube, Direction, Sample.Mip), L.Z));
		Weight += L.Z;
	}
	return ScaleVector(Sum, 1.f / std::max(Weight, 1e-8f));
}

FTextureAsset Prefilter(const FTextureAsset& InSource, unsigned InSize, unsigned InSamples)
{
	FTextureAsset Result;
	Result.Name = "GGX sky reflection";
	Result.Dimension = ETextureDimension::Cube;
	Result.Format = InSource.Format;
	const auto Levels = static_cast<unsigned>(std::bit_width(InSize));
	for (unsigned Level = 0; Level < Levels; ++Level)
	{
		const unsigned Size = InSize >> Level;
		const auto Samples =
		    FilterSamples(float(Level) / std::max(1U, Levels - 1), InSamples, InSource.Mips.front().Width);
		auto Mip = EnvironmentPrivate::MakeMip(Size, 6, Result.Format);
		for (unsigned Face = 0; Face < 6; ++Face)
		{
			for (unsigned Y = 0; Y < Size; ++Y)
			{
				for (unsigned X = 0; X < Size; ++X)
				{
					const auto N = CubeDirection(Face, (X + .5f) * 2 / Size - 1, (Y + .5f) * 2 / Size - 1);
					const auto Value =
					    Level ? Filter(InSource, N, Samples)
					          : SampleEnvironment(
					                InSource, N, std::max(0.f, std::log2(float(InSource.Mips.front().Width) / InSize)));
					Write(Mip, Result.Format, Face, X, Y, Value);
				}
			}
		}
		Result.Mips.push_back(std::move(Mip));
	}
	return Result;
}
} // namespace

FTextureAsset PrefilterEnvironment(const FTextureAsset& InCube, std::uint32_t InSize, std::uint32_t InSamples)
{
	ValidateTextureAsset(InCube);
	if (InCube.Dimension != ETextureDimension::Cube || InCube.Encoding != EMaterialTextureEncoding::Linear ||
	    !std::has_single_bit(InSize) || InSize > 256 || InSize > InCube.Mips.front().Width || !InSamples ||
	    InSamples > 1024)
	{
		throw std::invalid_argument("GGX prefilter requires a linear cube and bounded power-of-two output");
	}
	auto Result = Prefilter(InCube, InSize, InSamples);
	ValidateTextureAsset(Result);
	return Result;
}

FBakedEnvironment BakeEnvironment(std::span<const float> InRgba, std::uint32_t InWidth, std::uint32_t InHeight,
                                  FEnvironmentBakeSettings InSettings)
{
	if (!InWidth || !InHeight || InWidth != 2ULL * InHeight || InWidth > 16384 ||
	    InRgba.size() != std::uint64_t(InWidth) * InHeight * 4 || InRgba.size_bytes() > 512ULL * 1024 * 1024 ||
	    !std::has_single_bit(InSettings.RadianceSize) || InSettings.RadianceSize > 1024 ||
	    !std::has_single_bit(InSettings.SpecularSize) || InSettings.SpecularSize > 256 ||
	    InSettings.SpecularSize > InSettings.RadianceSize || !InSettings.Samples || InSettings.Samples > 1024)
	{
		throw std::invalid_argument("Sky requires a 2:1 HDR panorama and bounded power-of-two bake sizes");
	}
	ETextureFormat Format = ETextureFormat::Rgba16Float;
	for (std::size_t Index = 0; Index < InRgba.size(); ++Index)
	{
		const float Value = InRgba[Index];
		if (!std::isfinite(Value) || (Index % 4 != 3 && (Value < 0 || Value > 1e20f)))
		{
			throw std::invalid_argument("Sky radiance must be finite and nonnegative within the bake range");
		}
		if (Value > 65000.f)
		{
			Format = ETextureFormat::Rgba32Float;
		}
	}
	FBakedEnvironment Result;
	Result.Radiance = ConvertPanorama({InRgba, InWidth, InHeight}, InSettings.RadianceSize, Format);
	Result.Irradiance = ProjectEnvironmentSh(Result.Radiance);
	Result.Specular = PrefilterEnvironment(Result.Radiance, InSettings.SpecularSize, InSettings.Samples);
	return Result;
}
} // namespace Hyperion
