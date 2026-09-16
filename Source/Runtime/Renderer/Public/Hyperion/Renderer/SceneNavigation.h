#pragma once
#include "Hyperion/Renderer/SceneInstance.h"

namespace Hyperion
{
// Stateless navigation always reads the selected camera's current world pose.
std::optional<FSceneHandle> GetSceneNavigationCamera(const FSceneInstance& InScene);
FVec3 GetSceneNavigationPivot(const FSceneInstance& InScene);
void OrbitSceneCamera(FSceneInstance& InScene, float InYaw, float InPitch);
// Rotates the world-space camera basis while preserving eye position, lens and focus distance.
void RotateSceneCamera(FSceneInstance& InScene, float InYaw, float InPitch);
void DollySceneCamera(FSceneInstance& InScene, float InFactor, float InMinimum = .02f, float InMaximum = 100000.f,
                      std::optional<float> InFarPadding = {});
void PanSceneCamera(FSceneInstance& InScene, FVec3 InSteps);
void FitSceneCamera(FSceneInstance& InScene, float InAspect, bool bInModelViewerLens = false);
} // namespace Hyperion
