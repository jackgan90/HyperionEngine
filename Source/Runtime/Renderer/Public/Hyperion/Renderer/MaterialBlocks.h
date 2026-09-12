#pragma once
#include "Hyperion/Materials/MaterialBlocks.h"
#include "Hyperion/Renderer/MaterialPreparation.h"

namespace Hyperion
{
// Names identify an opt-in versioned ABI. Other shader blocks retain their arbitrary reflected layouts.
bool NormalizeStandardMaterialBlock(FShaderBinding& InBinding);
} // namespace Hyperion
