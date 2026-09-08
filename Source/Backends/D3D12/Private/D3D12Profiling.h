#pragma once
#include "D3D12Utilities.h"
#include "Hyperion/Core/ProfilingGpu.h"

namespace Hyperion
{
struct FD3D12DeviceState;
struct FD3D12RecordedList;

#if HYP_ENABLE_PROFILING
struct FD3D12ProfileQueries
{
	ComPtr<ID3D12QueryHeap> Heap;
	ComPtr<ID3D12Resource> Readback;
};

struct FD3D12ProfileState
{
	FProfileGpuContext Context;
	std::uint64_t PreviousCpu{};
	std::uint64_t PreviousGpu{};
	bool bFrameEnabled{};
};

void PrepareD3D12Profiling(FD3D12DeviceState& InDevice, FD3D12ProfileState& InState,
                           std::shared_ptr<FD3D12ProfileQueries>& InQueries);
void BeginD3D12Profile(FD3D12RecordedList& InList, const FD3D12ProfileState& InState,
                       const std::shared_ptr<FD3D12ProfileQueries>& InQueries);
void EndD3D12Profile(FD3D12RecordedList& InList);
void CollectD3D12Profiles(std::span<const FRecordedList> InLists);
#endif
} // namespace Hyperion
