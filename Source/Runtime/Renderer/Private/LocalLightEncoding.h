#pragma once
#include "Hyperion/Renderer/ClusteredLights.h"

namespace Hyperion
{
struct FLocalLightAttenuation
{
	float InverseRange{};
	float InnerCos{};
	float OuterCos{};
	float SpotFlag{};
};

inline FLocalLightAttenuation EncodeLocalLightAttenuation(const FLocalLight& InLight)
{
	return {1.f / InLight.Range, InLight.InnerCos, InLight.OuterCos, InLight.bSpot ? 1.f : 0.f};
}

inline FClusterLightData EncodeClusterLight(const FLocalLight& InLight)
{
	const auto Attenuation = EncodeLocalLightAttenuation(InLight);
	return {{InLight.Position.X, InLight.Position.Y, InLight.Position.Z, Attenuation.InverseRange},
	        {InLight.Radiance.X, InLight.Radiance.Y, InLight.Radiance.Z, Attenuation.SpotFlag},
	        {InLight.Direction.X, InLight.Direction.Y, InLight.Direction.Z, Attenuation.InnerCos},
	        {Attenuation.OuterCos, 0, 0, 0}};
}

inline FVec4 EncodeLightConeRange(const FLocalLight& InLight)
{
	const auto Attenuation = EncodeLocalLightAttenuation(InLight);
	return {Attenuation.InverseRange, Attenuation.InnerCos, Attenuation.OuterCos, Attenuation.SpotFlag};
}
} // namespace Hyperion
