#include "Hyperion/Scene/SceneLight.h"
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
bool FSceneDirectionalLight::operator==(const FSceneDirectionalLight& InOther) const
{
	return Color.X == InOther.Color.X && Color.Y == InOther.Color.Y && Color.Z == InOther.Color.Z &&
	       Intensity == InOther.Intensity && bCastShadows == InOther.bCastShadows;
}

bool FSceneEnvironmentLight::operator==(const FSceneEnvironmentLight& InOther) const
{
	return Color.X == InOther.Color.X && Color.Y == InOther.Color.Y && Color.Z == InOther.Color.Z &&
	       Intensity == InOther.Intensity && Source == InOther.Source && Sky == InOther.Sky &&
	       YawRadians == InOther.YawRadians && bVisible == InOther.bVisible && Data == InOther.Data;
}

FVec3 SceneLightRadiance(FVec3 InColor, float InIntensity)
{
	const auto Radiance = ScaleVector(InColor, InIntensity);
	if (!IsFinite(InColor) || !std::isfinite(InIntensity) || InColor.X < 0 || InColor.Y < 0 || InColor.Z < 0 ||
	    InIntensity < 0 || !IsFinite(Radiance))
	{
		throw std::invalid_argument("Scene light requires finite nonnegative color, intensity and radiance");
	}
	return Radiance;
}

void ValidateSceneDirectionalLight(const FSceneDirectionalLight& InLight)
{
	SceneLightRadiance(InLight.Color, InLight.Intensity);
}

void ValidateSceneEnvironmentLight(const FSceneEnvironmentLight& InLight)
{
	SceneLightRadiance(InLight.Color, InLight.Intensity);
	if (!std::isfinite(InLight.YawRadians) || (InLight.Source != ESceneEnvironmentSource::ConstantColor &&
	                                           InLight.Source != ESceneEnvironmentSource::SkyAsset))
	{
		throw std::invalid_argument("Invalid environment source or yaw");
	}
	if (InLight.Sky)
	{
		ValidateAssetRef(*InLight.Sky);
		if (InLight.Sky->TypeId != RecordType<FSkyAsset>().Id)
		{
			throw std::invalid_argument("Environment reference must target a sky asset");
		}
	}
	if (InLight.Source == ESceneEnvironmentSource::SkyAsset && !InLight.Sky)
	{
		throw std::invalid_argument("Sky environment requires an asset reference");
	}
}

template<> const FRecordDescriptor& RecordType<FSceneDirectionalLight>()
{
	static const auto Type = MakeRecord<FSceneDirectionalLight>(
	    "hyperion.scenedirectionallight",
	    {Member("color", &FSceneDirectionalLight::Color), Member("intensity", &FSceneDirectionalLight::Intensity),
	     Member("castShadows", &FSceneDirectionalLight::bCastShadows)},
	    1, ValidateSceneDirectionalLight);
	return Type;
}

template<> std::span<const ESceneEnvironmentSource> RecordEnumValues<ESceneEnvironmentSource>()
{
	static constexpr std::array Values{ESceneEnvironmentSource::ConstantColor, ESceneEnvironmentSource::SkyAsset};
	return Values;
}

template<> const FRecordDescriptor& RecordType<FSceneEnvironmentLight>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneEnvironmentLight>(
		    "hyperion.sceneenvironmentlight",
		    {Member("color", &FSceneEnvironmentLight::Color), Member("intensity", &FSceneEnvironmentLight::Intensity),
		     Member("source", &FSceneEnvironmentLight::Source), Member("sky", &FSceneEnvironmentLight::Sky),
		     Member("yawRadians", &FSceneEnvironmentLight::YawRadians),
		     Member("visible", &FSceneEnvironmentLight::bVisible)},
		    2, ValidateSceneEnvironmentLight);
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		return Result;
	}();
	return Type;
}
} // namespace Hyperion
