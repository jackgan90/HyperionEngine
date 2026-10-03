#pragma once
#include "Hyperion/RHI/RHITypes.h"

namespace Hyperion
{
ERHIShaderVisibility RHIShaderVisibility(EShaderStageMask InStages);
EShaderStageMask ShaderStages(ERHIShaderVisibility InVisibility);
ERHIBindingKind GetShaderBindingKind(const FShaderBinding& InBinding);
// Reflection coverage only; callers also validate pipeline state, layout and native device restrictions.
void ValidatePipelineBindings(const FPipelineDesc& InDesc, const FResourceBindingLayoutDesc& InLayout);
void ValidatePipelineBindings(const FComputePipelineDesc& InDesc, const FResourceBindingLayoutDesc& InLayout);
} // namespace Hyperion
