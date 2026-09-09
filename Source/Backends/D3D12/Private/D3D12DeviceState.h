#pragma once
#include "D3D12Descriptors.h"
#include "D3D12Utilities.h"
#include "Hyperion/RHI/RHICapabilities.h"
#include <D3D12MemAlloc.h>
#include <d3d12sdklayers.h>
#include <functional>
#include <mutex>

namespace Hyperion
{
struct FD3D12Buffer;

struct FD3D12DeviceState : std::enable_shared_from_this<FD3D12DeviceState>
{
	ComPtr<ID3D12Device> Device;
	ComPtr<D3D12MA::Allocator> Allocator;
	FD3D12DescriptorArena ResourceSources;
	FD3D12DescriptorArena ResourceTables;
	FD3D12DescriptorArena SamplerSources;
	FD3D12DescriptorArena SamplerTables;
	std::uint64_t DescriptorAllocations{};
	std::uint64_t DescriptorCopies{};
	std::uint64_t BindingSetsCreated{};
	std::atomic_uint64_t GraphicsRootBinds{};
	std::atomic_uint64_t GraphicsHeapBinds{};
	std::atomic_uint64_t GraphicsConstantBinds{};
	std::atomic_uint64_t GraphicsTableBinds{};
	std::atomic_uint64_t GraphicsPipelineBinds{};
	std::atomic_uint64_t GraphicsGeometryBinds{};
	std::atomic_uint64_t GraphicsDynamicBinds{};
	std::atomic_uint64_t CommandListsCreated{};
	std::atomic_uint64_t CommandListResets{};
	std::uint64_t PipelinesCreated{};
	std::uint64_t ConstantBytesWritten{};
	FGpuFrameTiming LastGpuTiming;
	FGpuTimingCapture GpuTimingCapture;
	std::size_t GpuTimingCapacity{};
	std::uint64_t GpuTimingEpoch{};
	ComPtr<IDXGIFactory6> Factory;
	ComPtr<IDXGIAdapter1> Adapter;
	ComPtr<ID3D12CommandQueue> Queue;
	ComPtr<ID3D12Fence> Fence;
	ComPtr<ID3D12InfoQueue> Info;
	HANDLE Event{};
	std::uint64_t NextFence = 1;
	std::uint64_t Submitted{};
	bool bDebug{};
	std::string AdapterName;
	FRHICapabilities Capabilities;
	~FD3D12DeviceState();
	void Wait(std::uint64_t InValue);
	std::uint64_t Signal();
	void Idle();

	struct FUploadBatch
	{
		ComPtr<ID3D12CommandAllocator> Allocator;
		ComPtr<ID3D12GraphicsCommandList> List;
		std::vector<ComPtr<ID3D12Resource>> Resources;
		std::vector<ComPtr<D3D12MA::Allocation>> Allocations;
		std::vector<ComPtr<ID3D12DescriptorHeap>> DescriptorHeaps;
		std::uint64_t FenceValue = UINT64_MAX;
	};

	std::vector<FUploadBatch> Uploads;

	struct FSubmission
	{
		std::uint64_t FenceValue{};
		std::vector<FRecordedList> Lists;
		std::uint64_t TimingCapture{};
	};

	std::vector<FSubmission> Submissions;
	void CollectUploads();
	std::shared_ptr<FD3D12Buffer> AllocateBuffer(std::uint64_t InBytes, D3D12_HEAP_TYPE InHeap,
	                                             D3D12_RESOURCE_STATES InInitial);
	void Immediate(const std::function<void(ID3D12GraphicsCommandList*)>& InRecord,
	               std::vector<ComPtr<ID3D12Resource>> InResources,
	               std::vector<ComPtr<D3D12MA::Allocation>> InAllocations);
};
} // namespace Hyperion
