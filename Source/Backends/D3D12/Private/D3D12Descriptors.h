#pragma once
#include "D3D12Utilities.h"
#include <mutex>

namespace Hyperion
{
struct FD3D12DescriptorRange
{
	UINT Offset = UINT_MAX;
	UINT Count{};
};

class FD3D12DescriptorArena
{
public:
	void Initialize(ID3D12Device& InDevice, D3D12_DESCRIPTOR_HEAP_TYPE InType, UINT InCapacity, bool bInVisible);
	FD3D12DescriptorRange Reserve(UINT InCount);
	void Release(FD3D12DescriptorRange InRange) noexcept;
	D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT InOffset) const;
	D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT InOffset) const;
	ID3D12DescriptorHeap* GetHeap() const;

private:
	ComPtr<ID3D12DescriptorHeap> Heap;
	std::vector<bool> Occupied;
	UINT Step{};
	bool bVisible{};
	std::mutex Mutex;
};
} // namespace Hyperion
