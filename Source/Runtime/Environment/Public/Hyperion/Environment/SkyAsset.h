#pragma once
#include "Hyperion/AssetTypes/AssetTypes.h"
#include "Hyperion/Math/Math.h"
#include "Hyperion/Textures/TextureAsset.h"

namespace Hyperion
{
// Orthonormal real SH, l = 0..2, RGB irradiance with cosine convolution included.
using FEnvironmentSh = std::array<std::array<float, 3>, 9>;

struct FSkyAsset
{
	std::string Name;
	FAssetRef Radiance;
	FAssetRef Specular;
	FAssetRef Brdf;
	FEnvironmentSh Irradiance{};
	std::uint32_t Convention = 1;
};

struct FEnvironmentBakeSettings
{
	std::uint32_t RadianceSize = 256;
	std::uint32_t SpecularSize = 64;
	std::uint32_t Samples = 256;
};

struct FBakedEnvironment
{
	FTextureAsset Radiance;
	FTextureAsset Specular;
	FEnvironmentSh Irradiance{};
};

void ValidateSkyAsset(const FSkyAsset& InAsset);
template<> const FRecordDescriptor& RecordType<FSkyAsset>();
FVec3 CubeDirection(unsigned InFace, float InU, float InV);
FVec3 SampleEnvironment(const FTextureAsset& InCube, FVec3 InDirection, float InMip = 0);
std::array<float, 9> EnvironmentShBasis(FVec3 InDirection);
FVec3 EvaluateEnvironmentSh(const FEnvironmentSh& InSh, FVec3 InNormal);
FEnvironmentSh ProjectEnvironmentSh(const FTextureAsset& InCube);
// Also accepts a renderer-captured cube for future reflection-probe baking.
FTextureAsset PrefilterEnvironment(const FTextureAsset& InCube, std::uint32_t InSize = 64,
                                   std::uint32_t InSamples = 256);
FBakedEnvironment BakeEnvironment(std::span<const float> InRgba, std::uint32_t InWidth, std::uint32_t InHeight,
                                  FEnvironmentBakeSettings InSettings = {});
FTextureAsset BuildEnvironmentBrdf(std::uint32_t InSize = 128, std::uint32_t InSamples = 256);
} // namespace Hyperion
