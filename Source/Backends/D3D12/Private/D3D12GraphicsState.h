#pragma once
#include "D3D12DeviceState.h"

namespace Hyperion
{
DXGI_FORMAT NativeDepthFormat(ERHIDepthFormat InFormat);
DXGI_FORMAT NativeVertexFormat(EVertexFormat InFormat);
D3D_PRIMITIVE_TOPOLOGY NativeTopology(ERHIPrimitiveTopology InTopology);
void ApplyGraphicsState(D3D12_GRAPHICS_PIPELINE_STATE_DESC& OutPso, const FPipelineDesc& InDesc);
void ValidatePipelineBindings(const FPipelineDesc& InDesc, const FResourceBindingLayoutDesc& InLayout);
} // namespace Hyperion
