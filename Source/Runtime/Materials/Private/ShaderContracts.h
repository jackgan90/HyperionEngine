#pragma once
#include "Hyperion/Materials/MaterialBlocks.h"

namespace Hyperion
{
FStandardMaterialBlock FindShaderUniformContract(
    std::string_view InName, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts);
FEngineMaterialResource FindShaderResourceContract(
    std::string_view InName, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts);
void ValidateShaderContracts(std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts);
} // namespace Hyperion
