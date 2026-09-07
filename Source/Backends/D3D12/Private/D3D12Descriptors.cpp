#include "D3D12Descriptors.h"
#include <cassert>

namespace Hyperion
{
void FD3D12DescriptorArena::Initialize(ID3D12Device& InDevice, D3D12_DESCRIPTOR_HEAP_TYPE InType, UINT InCapacity,
                                       bool bInVisible)
{
	if (Heap || InCapacity == 0)
	{
		throw std::invalid_argument("Invalid descriptor arena initialization");
	}
	D3D12_DESCRIPTOR_HEAP_DESC Desc{};
	Desc.Type = InType;
	Desc.NumDescriptors = InCapacity;
	Desc.Flags = bInVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	Check(InDevice.CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&Heap)), "Create material descriptor heap");
	Step = InDevice.GetDescriptorHandleIncrementSize(InType);
	bVisible = bInVisible;
	Occupied.resize(InCapacity);
}

FD3D12DescriptorRange FD3D12DescriptorArena::Reserve(UINT InCount)
{
	std::lock_guard Lock(Mutex);
	if (InCount == 0)
	{
		return {};
	}
	UINT Run{};
	for (UINT Index = 0; Index < Occupied.size(); ++Index)
	{
		Run = Occupied[Index] ? 0 : Run + 1;
		if (Run == InCount)
		{
			const UINT Begin = Index + 1 - InCount;
			for (UINT Slot = Begin; Slot <= Index; ++Slot)
			{
				Occupied[Slot] = true;
			}
			return {Begin, InCount};
		}
	}
	throw std::runtime_error("Material descriptor capacity exceeded (contiguous range unavailable)");
}

void FD3D12DescriptorArena::Release(FD3D12DescriptorRange InRange) noexcept
{
	if (InRange.Count == 0)
	{
		return;
	}
	std::lock_guard Lock(Mutex);
	assert(InRange.Offset <= Occupied.size() && InRange.Count <= Occupied.size() - InRange.Offset);
	for (UINT Index = 0; Index < InRange.Count; ++Index)
	{
		assert(Occupied[InRange.Offset + Index]);
		Occupied[InRange.Offset + Index] = false;
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE FD3D12DescriptorArena::Cpu(UINT InOffset) const
{
	if (InOffset >= Occupied.size())
	{
		throw std::out_of_range("Descriptor CPU offset exceeds heap");
	}
	D3D12_CPU_DESCRIPTOR_HANDLE Result = Heap->GetCPUDescriptorHandleForHeapStart();
	Result.ptr += std::size_t(InOffset) * Step;
	return Result;
}

D3D12_GPU_DESCRIPTOR_HANDLE FD3D12DescriptorArena::Gpu(UINT InOffset) const
{
	if (!bVisible || InOffset >= Occupied.size())
	{
		throw std::out_of_range("Descriptor GPU offset exceeds visible heap");
	}
	D3D12_GPU_DESCRIPTOR_HANDLE Result = Heap->GetGPUDescriptorHandleForHeapStart();
	Result.ptr += std::uint64_t(InOffset) * Step;
	return Result;
}

ID3D12DescriptorHeap* FD3D12DescriptorArena::GetHeap() const
{
	return Heap.Get();
}
} // namespace Hyperion
