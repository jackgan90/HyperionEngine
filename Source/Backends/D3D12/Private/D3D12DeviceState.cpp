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
	// This fence follows every submitted upload, including batches whose own Signal failed.
	Uploads.clear();
	Submissions.clear();
}

void FD3D12DeviceState::CollectUploads()
{
	const auto Completed = Fence->GetCompletedValue();
	std::erase_if(Uploads,
	              [Completed](const FUploadBatch& InBatch)
	              {
		              return InBatch.FenceValue <= Completed;
	              });
	std::erase_if(Submissions,
	              [Completed](const FSubmission& InSubmission)
	              {
		              return InSubmission.FenceValue <= Completed;
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

void FD3D12DeviceState::Immediate(const std::function<void(ID3D12GraphicsCommandList*)>& InRecord,
                                  std::vector<ComPtr<ID3D12Resource>> InResources,
                                  std::vector<ComPtr<D3D12MA::Allocation>> InAllocations)
{
	FUploadBatch Batch;
	Batch.Resources = std::move(InResources);
	Batch.Allocations = std::move(InAllocations);
	Check(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Batch.Allocator)),
	      "Upload allocator");
	Check(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Batch.Allocator.Get(), nullptr,
	                                IID_PPV_ARGS(&Batch.List)),
	      "Upload list");
	InRecord(Batch.List.Get());
	Check(Batch.List->Close(), "Close upload list");
	Uploads.push_back(std::move(Batch));
	auto& SubmittedUpload = Uploads.back();
	ID3D12CommandList* Lists[] = {SubmittedUpload.List.Get()};
	Queue->ExecuteCommandLists(1, Lists);
	SubmittedUpload.FenceValue = Signal();
	Wait(SubmittedUpload.FenceValue);
	CollectUploads();
}
} // namespace Hyperion
