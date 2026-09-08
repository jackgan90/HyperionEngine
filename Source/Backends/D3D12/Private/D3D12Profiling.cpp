#include "D3D12Profiling.h"
#include "D3D12Resources.h"

namespace Hyperion
{
#if HYP_ENABLE_PROFILING
namespace
{
std::shared_ptr<FD3D12ProfileQueries> CreateQueries(ID3D12Device& InDevice)
{
	auto Result = std::make_shared<FD3D12ProfileQueries>();
	D3D12_QUERY_HEAP_DESC Heap{};
	Heap.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	Heap.Count = ContextCount * 2;
	D3D12_HEAP_PROPERTIES Properties{};
	Properties.Type = D3D12_HEAP_TYPE_READBACK;
	const auto Buffer = BufferDesc(sizeof(std::uint64_t) * Heap.Count);
	if (FAILED(InDevice.CreateQueryHeap(&Heap, IID_PPV_ARGS(&Result->Heap))) ||
	    FAILED(InDevice.CreateCommittedResource(&Properties, D3D12_HEAP_FLAG_NONE, &Buffer,
	                                            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
	                                            IID_PPV_ARGS(&Result->Readback))))
	{
		return {};
	}
	return Result;
}
} // namespace

void PrepareD3D12Profiling(FD3D12DeviceState& InDevice, FD3D12ProfileState& InState,
                           std::shared_ptr<FD3D12ProfileQueries>& InQueries)
{
	InState.bFrameEnabled = false;
	if (!IsProfilingEnabled(EProfileCategory::Gpu) || !GetProfilingConnection() ||
	    (InQueries && InQueries.use_count() != 1))
	{
		return;
	}
	UINT64 Frequency{};
	UINT64 GpuTime{};
	UINT64 CpuTime{};
	if (FAILED(InDevice.Queue->GetTimestampFrequency(&Frequency)) || !Frequency ||
	    FAILED(InDevice.Queue->GetClockCalibration(&GpuTime, &CpuTime)))
	{
		return;
	}
	const bool bSameConnection = InState.Context.Connection == GetProfilingConnection();
	if (!InitializeProfileGpu(InState.Context, GpuTime, static_cast<float>(1e9 / Frequency)))
	{
		return;
	}
	LARGE_INTEGER CpuFrequency{};
	QueryPerformanceFrequency(&CpuFrequency);
	if (bSameConnection && CpuTime > InState.PreviousCpu && GpuTime > InState.PreviousGpu)
	{
		CalibrateProfileGpu(InState.Context, GpuTime,
		                    static_cast<std::int64_t>((CpuTime - InState.PreviousCpu) * (1e9 / CpuFrequency.QuadPart)));
	}
	InState.PreviousCpu = CpuTime;
	InState.PreviousGpu = GpuTime;
	if (!InQueries)
	{
		// Telemetry allocation failure must not fail a render frame.
		try
		{
			InQueries = CreateQueries(*InDevice.Device.Get());
		}
		catch (const std::bad_alloc&)
		{
			return;
		}
	}
	InState.bFrameEnabled = InQueries != nullptr;
}

void BeginD3D12Profile(FD3D12RecordedList& InList, const FD3D12ProfileState& InState,
                       const std::shared_ptr<FD3D12ProfileQueries>& InQueries)
{
	if (!InState.bFrameEnabled || !IsProfilingEnabled(EProfileCategory::Gpu))
	{
		return;
	}
	InList.ProfileQueries = InQueries;
	InList.ProfileContext = InState.Context;
	InList.ProfileSpan = BeginProfileGpu();
	InList.List->EndQuery(InQueries->Heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, InList.Context * 2);
}

void EndD3D12Profile(FD3D12RecordedList& InList)
{
	if (InList.ProfileQueries)
	{
		const auto& Queries = *InList.ProfileQueries;
		InList.List->EndQuery(Queries.Heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, InList.Context * 2 + 1);
		InList.List->ResolveQueryData(Queries.Heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, InList.Context * 2, 2,
		                              Queries.Readback.Get(), InList.Context * 2 * sizeof(std::uint64_t));
		EndProfileGpu(InList.ProfileSpan);
	}
}

void CollectD3D12Profiles(std::span<const FRecordedList> InLists)
{
	static constinit FProfileSite Site{"GraphicsPass", __func__, __FILE__, __LINE__};
	for (const auto& List : InLists)
	{
		auto* Native = dynamic_cast<FD3D12RecordedList*>(List.Payload.get());
		if (!Native || !Native->ProfileQueries)
		{
			continue;
		}
		auto Queries = std::move(Native->ProfileQueries);
		const SIZE_T Offset = Native->Context * 2 * sizeof(std::uint64_t);
		D3D12_RANGE Range{Offset, Offset + 2 * sizeof(std::uint64_t)};
		void* Data{};
		if (SUCCEEDED(Queries->Readback->Map(0, &Range, &Data)))
		{
			const auto* Times = static_cast<const std::uint64_t*>(Data) + Native->Context * 2;
			if (Times[0] && Times[1] >= Times[0])
			{
				PublishProfileGpu(Native->ProfileContext, Site, Native->ProfileSpan, Times[0], Times[1]);
			}
			D3D12_RANGE Written{};
			Queries->Readback->Unmap(0, &Written);
		}
	}
}
#endif
} // namespace Hyperion
