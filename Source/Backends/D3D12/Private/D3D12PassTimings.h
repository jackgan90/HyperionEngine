#pragma once
#include "D3D12Utilities.h"

namespace Hyperion
{
struct FD3D12DeviceState;
struct FD3D12RecordedList;

struct FD3D12PassQueries
{
	ComPtr<ID3D12QueryHeap> Heap;
	ComPtr<ID3D12Resource> Readback;
	double MillisecondsPerTick{};
};

std::shared_ptr<FD3D12PassQueries> CreatePassQueries(FD3D12DeviceState& InDevice);
void BeginPassTiming(FD3D12RecordedList& InList, const std::shared_ptr<FD3D12PassQueries>& InQueries);
void EndPassTiming(FD3D12RecordedList& InList);
void CollectPassTimings(FD3D12DeviceState& InDevice, std::span<const FRecordedList> InLists);
} // namespace Hyperion
