#pragma once
#include "Hyperion/Materials/Lighting/SceneLightingParameters.h"
#include "Hyperion/Materials/MaterialParameters.h"
#include "Hyperion/Renderer/ScenePublication.h"

namespace Hyperion
{
inline constexpr auto DirectionalLightsSemantic = ESceneLightingSemantic::DirectionalLights;
FMaterialValue DirectionalLightBuffer(const FSceneMetadata* InMetadata = nullptr,
                                      const FMaterialValue* InPrevious = nullptr);
} // namespace Hyperion
