#include "D3D12PassTimings.h"
#include "D3D12Resources.h"

namespace Hyperion
{
std::shared_ptr<FD3D12PassQueries> CreatePassQueries(FD3D12DeviceState& InDevice)
{
	UINT64 Frequency{};
	if (FAILED(InDevice.Queue->GetTimestampFrequency(&Frequency)) || !Frequency)
	{
		return {};
	}
	auto Result = std::make_shared<FD3D12PassQueries>();
	D3D12_QUERY_HEAP_DESC Desc{};
	Desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	Desc.Count = ContextCount * 2;
	D3D12_HEAP_PROPERTIES Heap{};
	Heap.Type = D3D12_HEAP_TYPE_READBACK;
	const auto Buffer = BufferDesc(Desc.Count * sizeof(UINT64));
	if (FAILED(InDevice.Device->CreateQueryHeap(&Desc, IID_PPV_ARGS(&Result->Heap))) ||
	    FAILED(InDevice.Device->CreateCommittedResource(&Heap, D3D12_HEAP_FLAG_NONE, &Buffer,
	                                                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
	                                                    IID_PPV_ARGS(&Result->Readback))))
	{
		return {};
	}
	Result->MillisecondsPerTick = 1000.0 / Frequency;
	return Result;
}

void BeginPassTiming(FD3D12RecordedList& InList, const std::shared_ptr<FD3D12PassQueries>& InQueries)
{
	InList.TimingQueries = InQueries;
	if (InQueries)
	{
		InList.List->EndQuery(InQueries->Heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, InList.Context * 2);
	}
}

void EndPassTiming(FD3D12RecordedList& InList)
{
	if (InList.TimingQueries)
	{
		const auto& Queries = *InList.TimingQueries;
		InList.List->EndQuery(Queries.Heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, InList.Context * 2 + 1);
		InList.List->ResolveQueryData(Queries.Heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, InList.Context * 2, 2,
		                              Queries.Readback.Get(), InList.Context * 2 * sizeof(UINT64));
	}
}

void CollectPassTimings(FD3D12DeviceState& InDevice, std::span<const FRecordedList> InLists,
                        std::uint64_t InCaptureEpoch)
{
	FGpuFrameTiming Result;
	std::size_t Expected{};
	for (const auto& List : InLists)
	{
		auto* Native = dynamic_cast<FD3D12RecordedList*>(List.Payload.get());
		if (!Native || !Native->TimingQueries)
		{
			continue;
		}
		// Called only after the submission fence completes. Moving avoids duplicate collection.
		++Expected;
		auto Queries = std::move(Native->TimingQueries);
		const SIZE_T Offset = Native->Context * 2 * sizeof(UINT64);
		D3D12_RANGE Range{Offset, Offset + 2 * sizeof(UINT64)};
		void* Mapped{};
		if (SUCCEEDED(Queries->Readback->Map(0, &Range, &Mapped)))
		{
			const auto* Times = static_cast<const UINT64*>(Mapped) + Native->Context * 2;
			if (Times[1] >= Times[0] && Times[0])
			{
				Result.Frame = Native->Frame;
				Result.Swapchain = Native->Owner;
				Result.Passes.push_back({Native->Name, (Times[1] - Times[0]) * Queries->MillisecondsPerTick});
			}
			D3D12_RANGE Written{};
			Queries->Readback->Unmap(0, &Written);
		}
	}
	if (Expected && InDevice.GpuTimingCapacity && InCaptureEpoch == InDevice.GpuTimingEpoch)
	{
		if (Result.Passes.size() == Expected && InDevice.GpuTimingCapture.Frames.size() < InDevice.GpuTimingCapacity)
		{
			InDevice.GpuTimingCapture.Frames.push_back(Result);
		}
		else
		{
			++InDevice.GpuTimingCapture.DroppedFrames;
		}
	}
	if (!Result.Passes.empty())
	{
		InDevice.LastGpuTiming = std::move(Result);
	}
}

} // namespace Hyperion
