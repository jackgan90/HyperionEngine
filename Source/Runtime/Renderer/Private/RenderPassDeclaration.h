#pragma once
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
std::vector<FRenderTargetSource> CollectMaterialReads(const FRenderSceneSnapshot& InSnapshot);
} // namespace Hyperion
