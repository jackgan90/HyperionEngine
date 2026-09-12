#pragma once
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
struct FSceneDirectionalLight
{
	FVec3 Color{1, 1, 1};
	float Intensity = 1;
	bool bCastShadows = true;
	bool operator==(const FSceneDirectionalLight& InOther) const;
};

struct FSceneEnvironmentLight
{
	FVec3 Color{1, 1, 1};
	float Intensity = 1;
	bool operator==(const FSceneEnvironmentLight& InOther) const;
};

FVec3 SceneLightRadiance(FVec3 InColor, float InIntensity);
void ValidateSceneDirectionalLight(const FSceneDirectionalLight& InLight);
void ValidateSceneEnvironmentLight(const FSceneEnvironmentLight& InLight);
template<> const FRecordDescriptor& RecordType<FSceneDirectionalLight>();
template<> const FRecordDescriptor& RecordType<FSceneEnvironmentLight>();
} // namespace Hyperion
