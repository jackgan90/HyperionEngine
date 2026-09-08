#pragma once
#include "D3D12Resources.h"

namespace Hyperion
{
// One instance per recording. Root changes invalidate all arguments; heap changes invalidate tables.
struct FD3D12GraphicsBindingState
{
	ID3D12RootSignature* Root{};
	std::array<ID3D12DescriptorHeap*, 2> Heaps{};
	std::array<D3D12_GPU_VIRTUAL_ADDRESS, 64> Constants{};
	std::array<UINT64, 64> Tables{};
	std::uint64_t RootBinds{};
	std::uint64_t HeapBinds{};
	std::uint64_t ConstantBinds{};
	std::uint64_t TableBinds{};
};

void ValidateGraphicsBindings(const FDrawPacket& InDraw, const FD3D12Pipeline& InPipeline,
                              const FD3D12DeviceState& InState);
void RecordGraphicsBindings(ID3D12GraphicsCommandList& InList, const FDrawPacket& InDraw,
                            const FD3D12Pipeline& InPipeline, const FD3D12DeviceState& InState,
                            FD3D12GraphicsBindingState& InBindings);
} // namespace Hyperion
