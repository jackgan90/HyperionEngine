#pragma once
#include "Hyperion/Math/Ray.h"
#include "Hyperion/Scene/SceneCamera.h"

namespace Hyperion
{
FMat4 SceneCameraViewProjection(const FSceneCameraPose& InPose, const FSceneCamera& InLens, float InAspect,
                                EDepthConvention InDepth);
// Coordinates are normalized over the image, with (0,0) at top left. Interval is world distance.
std::optional<FRay> MakeViewportRay(const FSceneCameraView& InCamera, FVec2 InPosition, std::uint32_t InWidth,
                                    std::uint32_t InHeight, EDepthConvention InDepth = EDepthConvention::Standard);
} // namespace Hyperion
