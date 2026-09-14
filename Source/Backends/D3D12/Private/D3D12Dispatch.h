#pragma once
#include "D3D12Resources.h"

namespace Hyperion
{
void ValidateTextureAccess(const FTextureView& InView, EResourceState InState, const FPassCommands& InCommands);
void ValidateBufferAccess(const FReadBufferView& InView, EResourceState InState, const FPassCommands& InCommands);
void ValidateResourceAccesses(const FD3D12DeviceState& InState, const FPassCommands& InCommands);
void ValidateDispatches(const FD3D12DeviceState& InState, const std::shared_ptr<const FPassCommands>& InOwnedCommands);
void RecordDispatches(ID3D12GraphicsCommandList& InList, const FD3D12DeviceState& InState,
                      const FPassCommands& InCommands);
} // namespace Hyperion
