#include "D3D12Resources.h"

namespace Hyperion
{
FD3D12DeviceState::~FD3D12DeviceState()
{
	if (Event)
	{
		CloseHandle(Event);
	}
}

void FD3D12DeviceState::Wait(std::uint64_t InValue)
{
	if (Fence->GetCompletedValue() < InValue)
	{
		Check(Fence->SetEventOnCompletion(InValue, Event), "Fence event");
		if (WaitForSingleObject(Event, 30000) != WAIT_OBJECT_0)
		{
			throw std::runtime_error("GPU fence timed out or device was removed");
		}
	}
}

std::uint64_t FD3D12DeviceState::Signal()
{
	auto Value = NextFence++;
	Check(Queue->Signal(Fence.Get(), Value), "Signal GPU fence");
	return Value;
}

void FD3D12DeviceState::Idle()
{
	Wait(Signal());
	CollectUploads();
}

void FD3D12DeviceState::CollectUploads()
{
	const auto Completed = Fence->GetCompletedValue();
	std::erase_if(Uploads,
	              [Completed](const FUploadBatch& InBatch)
	              {
		              return InBatch.FenceValue <= Completed;
	              });
}

std::shared_ptr<FD3D12Buffer> FD3D12DeviceState::AllocateBuffer(std::uint64_t InBytes, D3D12_HEAP_TYPE InHeap,
                                                                D3D12_RESOURCE_STATES InInitial)
{
	auto R = std::make_shared<FD3D12Buffer>();
	R->State = shared_from_this();
	R->Size = InBytes;
	D3D12MA::ALLOCATION_DESC A{};
	A.HeapType = InHeap;
	auto D = BufferDesc(InBytes);
	Check(Allocator->CreateResource(&A, &D, InInitial, nullptr, &R->Allocation, IID_PPV_ARGS(&R->Resource)),
	      "Allocate GPU buffer");
	return R;
}

void FD3D12DeviceState::Immediate(const std::function<void(ID3D12GraphicsCommandList*)>& InRecord)
{
	ComPtr<ID3D12CommandAllocator> A;
	ComPtr<ID3D12GraphicsCommandList> L;
	Check(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&A)), "Upload allocator");
	Check(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, A.Get(), nullptr, IID_PPV_ARGS(&L)),
	      "Upload list");
	InRecord(L.Get());
	Check(L->Close(), "Close upload list");
	ID3D12CommandList* Lists[] = {L.Get()};
	Queue->ExecuteCommandLists(1, Lists);
	Wait(Signal());
}
} // namespace Hyperion
