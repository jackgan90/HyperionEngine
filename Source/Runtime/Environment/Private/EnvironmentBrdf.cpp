#include "EnvironmentSampling.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
float Smith(float InCosine, float InAlphaSquared)
{
	return 2 * InCosine /
	       std::max(InCosine + std::sqrt(InAlphaSquared + (1 - InAlphaSquared) * InCosine * InCosine), 1e-7f);
}

FVec2 Integrate(float InNv, float InRoughness, unsigned InSamples)
{
	const FVec3 V{std::sqrt(1 - InNv * InNv), 0, InNv};
	const float A = InRoughness * InRoughness;
	FVec2 Result;
	for (unsigned Index = 0; Index < InSamples; ++Index)
	{
		const auto H = EnvironmentPrivate::GgxHalf(Index, InSamples, InRoughness);
		const float Vh = std::max(0.f, Dot(V, H));
		const auto L = Subtract(ScaleVector(H, 2 * Vh), V);
		if (L.Z > 0)
		{
			const float Visibility = Smith(InNv, A * A) * Smith(L.Z, A * A) * Vh / std::max(H.Z * InNv, 1e-7f);
			const float Fresnel = std::pow(1 - Vh, 5.f);
			Result.X += (1 - Fresnel) * Visibility;
			Result.Y += Fresnel * Visibility;
		}
	}
	return {Result.X / InSamples, Result.Y / InSamples};
}
} // namespace

FTextureAsset BuildEnvironmentBrdf(std::uint32_t InSize, std::uint32_t InSamples)
{
	if (!InSize || InSize > 256 || !InSamples || InSamples > 1024)
	{
		throw std::invalid_argument("Environment BRDF quality exceeds bake budget");
	}
	FTextureAsset Result;
	Result.Name = "GGX Smith environment BRDF V1";
	Result.Format = ETextureFormat::Rgba16Float;
	for (auto Size = InSize;; Size = std::max(1U, Size / 2))
	{
		auto Mip = EnvironmentPrivate::MakeMip(Size, 1, Result.Format);
		for (unsigned Y = 0; Y < Size; ++Y)
		{
			for (unsigned X = 0; X < Size; ++X)
			{
				const auto Value = Integrate((X + .5f) / Size, (Y + .5f) / Size, InSamples);
				WriteTexturePixel(Mip, Result.Format, std::size_t(Y) * Size + X, {Value.X, Value.Y, 0, 1});
			}
		}
		Result.Mips.push_back(std::move(Mip));
		if (Size == 1)
		{
			break;
		}
	}
	ValidateTextureAsset(Result);
	return Result;
}
} // namespace Hyperion
