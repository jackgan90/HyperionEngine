#pragma once
#include "D3D12Resources.h"

namespace Hyperion
{
void ValidateGraphicsBindings(const FDrawPacket& InDraw, const FD3D12Pipeline& InPipeline,
                              const FD3D12DeviceState& InState);
void RecordGraphicsBindings(ID3D12GraphicsCommandList& InList, const FDrawPacket& InDraw,
                            const FD3D12Pipeline& InPipeline, const FD3D12DeviceState& InState);
} // namespace Hyperion
