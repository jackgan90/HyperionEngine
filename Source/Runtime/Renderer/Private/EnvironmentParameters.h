#pragma once
#include "Hyperion/Materials/MaterialParameters.h"
#include "Hyperion/Scene/SceneLight.h"

namespace Hyperion
{
FMaterialParameterValues EnvironmentParameters(const FSceneEnvironmentLight* InLight = nullptr);
} // namespace Hyperion
