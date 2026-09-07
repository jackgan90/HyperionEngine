#pragma once
#include "Hyperion/RHI/RHITypes.h"
#include "Hyperion/Renderer/MaterialPreparation.h"

namespace Hyperion
{
FGraphicsState ConvertMaterialState(const FMaterialState& InState, bool bInMirrored = false);
FGraphicsDynamicState ConvertMaterialDynamicState(const FMaterialDynamicState& InState);
FResourceBindingLayoutDesc DescribeMaterialLayout(const FCompiledMaterialPass& InPass);
FPipelineDesc DescribeMaterialPipeline(const FCompiledMaterialPass& InProgram, const FMaterialPass& InPass,
                                       const FResourceBindingLayout& InLayout,
                                       std::vector<FVertexAttribute> InAttributes, std::uint32_t InStride,
                                       ERHIPrimitiveTopology InTopology, FGraphicsTarget InTarget,
                                       bool bInMirrored = false);
} // namespace Hyperion
