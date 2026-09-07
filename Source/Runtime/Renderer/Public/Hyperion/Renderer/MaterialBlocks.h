#pragma once
#include "Hyperion/Renderer/MaterialPreparation.h"

namespace Hyperion
{
// Names identify an opt-in versioned ABI. Other shader blocks retain their arbitrary reflected layouts.
bool NormalizeStandardMaterialBlock(FShaderBinding& InBinding);
std::vector<FMaterialParameterDeclaration> GetStandardMaterialBlockParameters(
    std::string_view InBlock, const FMaterialSemanticRegistry& InRegistry);
} // namespace Hyperion
