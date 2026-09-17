#pragma once
#include "Hyperion/Renderer/SceneInstance.h"

namespace Hyperion
{
// Stateless navigation always reads the selected camera's current world pose.
std::optional<FSceneHandle> GetSceneNavigationCamera(const FSceneInstance& InScene);
bool GetSceneCameraView(const FSceneInstance& InScene, FSceneCameraView& OutView);
// Presets need only the logical document; bounds fitting waits for CPU model success or failure.
bool CanInitializeSceneBrowsingView(const FSceneInstance& InScene);
FSceneCameraView MakeSceneBrowsingView(const FSceneInstance& InScene, float InAspect);
void OrbitSceneCamera(FSceneCameraView& InCamera, float InYaw, float InPitch);
void RotateSceneCamera(FSceneCameraView& InCamera, float InYaw, float InPitch);
void DollySceneCamera(FSceneCameraView& InCamera, float InFactor, float InMinimum = .02f, float InMaximum = 100000.f,
                      std::optional<float> InFarPadding = {});
void PanSceneCamera(FSceneCameraView& InCamera, FVec3 InSteps);
void FitSceneCamera(FSceneCameraView& InCamera, const FSceneInstance& InScene, float InAspect,
                    bool bInModelViewerLens = false);
FVec3 GetSceneNavigationPivot(const FSceneInstance& InScene);
void OrbitSceneCamera(FSceneInstance& InScene, float InYaw, float InPitch);
// Rotates the world-space camera basis while preserving eye position, lens and focus distance.
void RotateSceneCamera(FSceneInstance& InScene, float InYaw, float InPitch);
void DollySceneCamera(FSceneInstance& InScene, float InFactor, float InMinimum = .02f, float InMaximum = 100000.f,
                      std::optional<float> InFarPadding = {});
void PanSceneCamera(FSceneInstance& InScene, FVec3 InSteps);
void FitSceneCamera(FSceneInstance& InScene, float InAspect, bool bInModelViewerLens = false);
} // namespace Hyperion
