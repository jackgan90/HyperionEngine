#pragma once
#include "Hyperion/RenderControls/ShadowControls.h"
#include "Hyperion/Renderer/ScenePublication.h"

namespace Hyperion
{
// Applies only authored values; otherwise leaves caller-owned legacy defaults intact.
void ResolveSceneLightShadows(const FSceneMetadata* InMetadata, FCascadedShadowSettings& InOutDirectional,
                              FContactShadowSettings& InOutContact);
} // namespace Hyperion
