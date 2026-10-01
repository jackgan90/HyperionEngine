#include "Hyperion/Materials/Lighting/EnvironmentParameters.h"
#include "EnvironmentParameters.h"
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
std::shared_ptr<const FMaterialTextureSource> BlackTexture(bool bInCube)
{
	FTextureAsset Texture;
	Texture.Dimension = bInCube ? ETextureDimension::Cube : ETextureDimension::Texture2D;
	Texture.Format = ETextureFormat::Rgba16Float;
	Texture.Mips.push_back({1, 1, std::vector<std::uint8_t>(8 * (bInCube ? 6 : 1))});
	return std::make_shared<const FMaterialTextureSource>(std::make_shared<const FTextureAsset>(std::move(Texture)));
}
} // namespace

float EnvironmentYawRadians(float InDegrees)
{
	// Authored yaw accepts every finite float. Reduce degrees before converting to avoid overflow.
	return static_cast<float>(std::remainder(double(InDegrees), 360.0) * (std::numbers::pi / 180.0));
}

FMaterialParameterValues EnvironmentParameters(const FSceneEnvironmentLight* InLight)
{
	static const auto Cube = BlackTexture(true);
	static const auto Brdf = BlackTexture(false);
	const auto Data = InLight && InLight->Source == ESceneEnvironmentSource::SkyAsset ? InLight->Data : nullptr;
	const float Yaw = InLight ? EnvironmentYawRadians(InLight->YawDegrees) : 0;
	const FVec3 Tint = Data ? InLight->Tint : FVec3{};
	FMaterialParameterValues Result;
	const auto Set = [&](FMaterialSemanticId InSemantic, FMaterialValue InValue)
	{
		Result.push_back({InSemantic, std::move(InValue)});
	};
	Set(EEnvironmentV1Field::EnvironmentControl,
	    FMaterialValue::Float(Data ? FVec4{1, InLight->Intensity, float(Data->Textures[1]->GetMips().size() - 1), 0}
	                               : FVec4{}));
	Set(EEnvironmentV1Field::EnvironmentRotation, FMaterialValue::Float(FVec4{std::cos(Yaw), std::sin(Yaw), 0, 0}));
	// Diffuse tint is premultiplied into SH; the unused w lanes of SH0-SH2 carry the specular tint.
	const std::array ShSemantics{
	    EEnvironmentV1Field::EnvironmentSh0, EEnvironmentV1Field::EnvironmentSh1, EEnvironmentV1Field::EnvironmentSh2,
	    EEnvironmentV1Field::EnvironmentSh3, EEnvironmentV1Field::EnvironmentSh4, EEnvironmentV1Field::EnvironmentSh5,
	    EEnvironmentV1Field::EnvironmentSh6, EEnvironmentV1Field::EnvironmentSh7, EEnvironmentV1Field::EnvironmentSh8};
	const std::array<float, 3> SpecularTint{Tint.X, Tint.Y, Tint.Z};
	for (unsigned Index = 0; Index < 9; ++Index)
	{
		const auto Sh = Data ? Data->Irradiance[Index] : std::array<float, 3>{};
		Set(ShSemantics[Index], FMaterialValue::Float(FVec4{Sh[0] * Tint.X, Sh[1] * Tint.Y, Sh[2] * Tint.Z,
		                                                    Index < 3 ? SpecularTint[Index] : 0}));
	}
	Set(EEnvironmentSemantic::EnvironmentSpecular, FMaterialValue::FromTexture(Data ? Data->Textures[1] : Cube));
	Set(EEnvironmentSemantic::EnvironmentBrdf, FMaterialValue::FromTexture(Data ? Data->Textures[2] : Brdf));
	FMaterialSampler Sampler;
	Sampler.U = EMaterialAddressMode::Clamp;
	Sampler.V = EMaterialAddressMode::Clamp;
	Sampler.W = EMaterialAddressMode::Clamp;
	Set(EEnvironmentSemantic::EnvironmentSampler, FMaterialValue::FromSampler(Sampler));
	return Result;
}
} // namespace Hyperion
