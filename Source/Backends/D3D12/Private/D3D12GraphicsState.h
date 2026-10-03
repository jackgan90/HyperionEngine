#pragma once
#include "D3D12DeviceState.h"
#include "Hyperion/RHI/RHIBindingContracts.h"

namespace Hyperion
{
DXGI_FORMAT NativeColorFormat(ERHIColorFormat InFormat);
DXGI_FORMAT NativeDepthFormat(ERHIDepthFormat InFormat);
DXGI_FORMAT NativeVertexFormat(EVertexFormat InFormat);
D3D_PRIMITIVE_TOPOLOGY NativeTopology(ERHIPrimitiveTopology InTopology);
void ApplyGraphicsState(D3D12_GRAPHICS_PIPELINE_STATE_DESC& OutPso, const FPipelineDesc& InDesc);
} // namespace Hyperion
