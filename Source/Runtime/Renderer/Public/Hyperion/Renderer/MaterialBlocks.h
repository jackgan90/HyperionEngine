#pragma once
#include "Hyperion/Materials/MaterialBlocks.h"
#include "Hyperion/Renderer/MaterialPreparation.h"

namespace Hyperion
{
// Names identify an opt-in versioned ABI. Other shader blocks retain their arbitrary reflected layouts.
bool NormalizeStandardMaterialBlock(
    FShaderBinding& InBinding, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts = {});
FEngineMaterialResource ValidateEngineMaterialResource(
    const FShaderBinding& InBinding,
    std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts = {});
} // namespace Hyperion
