#pragma once
#include "Hyperion/Scene/SceneQuery.h"

namespace Hyperion
{
float SceneRayMaximum(const FSceneRayResult& InResult, float InMaximum);
void RaycastSceneModel(const FSceneModel& InModel, FSceneHandle InHandle, FRay InRay, const FSceneRayOptions& InOptions,
                       FSceneRayResult& OutResult);
} // namespace Hyperion
