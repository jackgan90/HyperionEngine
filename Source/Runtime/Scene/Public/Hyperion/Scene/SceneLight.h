#pragma once
#include "Hyperion/Environment/SkyAsset.h"
#include "Hyperion/Materials/MaterialResources.h"
#include "Hyperion/Scene/LightShadows.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
struct FSceneDirectionalLight
{
	FVec3 Color{1, 1, 1};
	float Intensity = 1;
	bool bCastShadows = true;
	std::optional<FSceneLightShadowSettings> ShadowSettings;
	std::int32_t Priority{};
	bool operator==(const FSceneDirectionalLight& InOther) const;
};

enum class ESceneEnvironmentSource
{
	ConstantColor,
	SkyAsset
};

// Immutable CPU sources shared by scene publication; native resources remain renderer-owned.
struct FSceneSkyData
{
	std::string Name;
	FAssetRef Reference;
	FEnvironmentSh Irradiance{};
	std::array<std::shared_ptr<const FMaterialTextureSource>, 3> Textures;
	std::vector<FAssetRef> Dependencies;
};

// Color applies to ConstantColor only. Tint, yaw and background visibility apply to SkyAsset only.
struct FSceneEnvironmentLight
{
	FVec3 Color{1, 1, 1};
	float Intensity = 1;
	ESceneEnvironmentSource Source = ESceneEnvironmentSource::SkyAsset;
	FAssetRef Sky = DefaultSkyReference();
	// Linear per-channel multiplier of sky radiance: background, diffuse SH and specular IBL.
	FVec3 Tint{1, 1, 1};
	float YawDegrees{};
	bool bVisible = true;
	std::shared_ptr<const FSceneSkyData> Data;
	std::int32_t Priority{};
	bool operator==(const FSceneEnvironmentLight& InOther) const;
};

struct FScenePointLight
{
	FVec3 Color{1, 1, 1};
	float Intensity = 10;
	float Range = 5;
	bool operator==(const FScenePointLight& InOther) const;
};

struct FSceneSpotLight
{
	FVec3 Color{1, 1, 1};
	float Intensity = 10;
	float Range = 5;
	float InnerRadians = .35f;
	float OuterRadians = .6f;
	bool operator==(const FSceneSpotLight& InOther) const;
};

void ValidateScenePointLight(const FScenePointLight& InLight);
void ValidateSceneSpotLight(const FSceneSpotLight& InLight);
template<> const FRecordDescriptor& RecordType<FScenePointLight>();
template<> const FRecordDescriptor& RecordType<FSceneSpotLight>();

FVec3 SceneLightRadiance(FVec3 InColor, float InIntensity);
void ValidateSceneDirectionalLight(const FSceneDirectionalLight& InLight);
void ValidateSceneEnvironmentLight(const FSceneEnvironmentLight& InLight);
template<> const FRecordDescriptor& RecordType<FSceneDirectionalLight>();
template<> const FRecordDescriptor& RecordType<FSceneEnvironmentLight>();
template<> std::span<const TRecordEnumEntry<ESceneEnvironmentSource>> RecordEnumEntries<ESceneEnvironmentSource>();
} // namespace Hyperion
