#pragma once
#include "D3D12Resources.h"

namespace Hyperion
{
struct FD3D12DrawPlan;

// One weak stream per recording context. Entries never extend resource or constant-page lifetimes.
struct FD3D12DrawCache
{
	std::mutex Mutex;
	std::weak_ptr<const std::vector<FDrawPacket>> Owner;
	std::weak_ptr<IRHITexture> DepthTarget;
	std::array<unsigned, 5> Target{};
	std::shared_ptr<const FD3D12DrawPlan> Plan;
};

std::shared_ptr<const FD3D12DrawPlan> PrepareNativeDraws(const FD3D12DeviceState& InState,
                                                         const std::shared_ptr<const FPassCommands>& InOwnedCommands,
                                                         FD3D12DrawCache& InCache);
void RecordNativeDrawPlan(ID3D12GraphicsCommandList& InList, const FD3D12DrawPlan& InPlan, FD3D12DeviceState& InState);
void ValidateDraws(const FD3D12DeviceState& InState, const FPassCommands& InCommands);
void RecordDraws(ID3D12GraphicsCommandList& InList, const FPassCommands& InCommands, FD3D12DeviceState& InState);
} // namespace Hyperion
