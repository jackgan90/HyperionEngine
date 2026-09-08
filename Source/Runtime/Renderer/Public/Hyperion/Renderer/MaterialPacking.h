#pragma once
#include "Hyperion/Renderer/MaterialParameterTable.h"
#include "Hyperion/Renderer/MaterialPreparation.h"

namespace Hyperion
{
// Packs logical row-major CPU values using the actual target's offsets/strides/major order.
// The caller supplies only active logical values. All remaining bytes, including padding, are zeroed.
std::vector<std::byte> PackMaterialConstants(const FMaterialProgramBinding& InBinding,
                                             std::span<const std::optional<FMaterialValue>> InValues);
std::vector<std::byte> PackMaterialConstants(const FMaterialProgramBinding& InBinding,
                                             std::span<const std::shared_ptr<const FMaterialValue>> InValues);
std::vector<std::byte> PackMaterialConstants(const FMaterialProgramBinding& InBinding,
                                             const FMaterialValueTable& InValues);
} // namespace Hyperion
