#include "EnvironmentParameters.h"
#include <cmath>

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

FMaterialParameterValues EnvironmentParameters(const FSceneEnvironmentLight* InLight)
{
	static const auto Cube = BlackTexture(true);
	static const auto Brdf = BlackTexture(false);
	const auto Data = InLight && InLight->Source == ESceneEnvironmentSource::SkyAsset ? InLight->Data : nullptr;
	const float Yaw = InLight ? InLight->YawRadians : 0;
	FMaterialParameterValues Result;
	const auto Set = [&](std::string InName, FMaterialValue InValue)
	{
		Result.push_back({"Engine.Scene." + InName, std::move(InValue)});
	};
	Set("EnvironmentControl",
	    FMaterialValue::Float(Data ? FVec4{1, InLight->Intensity, float(Data->Textures[1]->GetMips().size() - 1), 0}
	                               : FVec4{}));
	Set("EnvironmentRotation", FMaterialValue::Float(FVec4{std::cos(Yaw), std::sin(Yaw), 0, 0}));
	for (unsigned Index = 0; Index < 9; ++Index)
	{
		const auto Sh = Data ? Data->Irradiance[Index] : std::array<float, 3>{};
		Set("EnvironmentSh" + std::to_string(Index), FMaterialValue::Float(FVec4{Sh[0], Sh[1], Sh[2], 0}));
	}
	Set("EnvironmentSpecular", FMaterialValue::FromTexture(Data ? Data->Textures[1] : Cube));
	Set("EnvironmentBrdf", FMaterialValue::FromTexture(Data ? Data->Textures[2] : Brdf));
	FMaterialSampler Sampler;
	Sampler.U = EMaterialAddressMode::Clamp;
	Sampler.V = EMaterialAddressMode::Clamp;
	Sampler.W = EMaterialAddressMode::Clamp;
	Set("EnvironmentSampler", FMaterialValue::FromSampler(Sampler));
	return Result;
}
} // namespace Hyperion
