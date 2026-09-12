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
	       Intensity == InOther.Intensity;
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

template<> const FRecordDescriptor& RecordType<FSceneEnvironmentLight>()
{
	static const auto Type = MakeRecord<FSceneEnvironmentLight>(
	    "hyperion.sceneenvironmentlight",
	    {Member("color", &FSceneEnvironmentLight::Color), Member("intensity", &FSceneEnvironmentLight::Intensity)}, 1,
	    ValidateSceneEnvironmentLight);
	return Type;
}
} // namespace Hyperion
