#pragma once
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
	ComPtr<ID3D12DescriptorHeap> Textures;
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
		std::uint64_t FenceValue = UINT64_MAX;
	};

	std::vector<FUploadBatch> Uploads;

	struct FSubmission
	{
		std::uint64_t FenceValue{};
		std::vector<FRecordedList> Lists;
	};

	std::vector<FSubmission> Submissions;
	void CollectUploads();
	std::shared_ptr<FD3D12Buffer> AllocateBuffer(std::uint64_t InBytes, D3D12_HEAP_TYPE InHeap,
	                                             D3D12_RESOURCE_STATES InInitial);
	void Immediate(const std::function<void(ID3D12GraphicsCommandList*)>& InRecord);
	UINT TextureStep{};
	std::mutex DescriptorsMutex;
	std::array<bool, TextureCount> Occupied{};

	UINT Reserve()
	{
		std::lock_guard Lock(DescriptorsMutex);
		for (UINT I = 0; I < TextureCount; ++I)
		{
			if (!Occupied[I])
			{
				Occupied[I] = true;
				return I;
			}
		}
		throw std::runtime_error("Texture descriptor capacity exceeded");
	}

	void Release(UINT InI)
	{
		std::lock_guard Lock(DescriptorsMutex);
		Occupied[InI] = false;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT InI) const
	{
		auto H = Textures->GetCPUDescriptorHandleForHeapStart();
		H.ptr += std::size_t(InI) * TextureStep;
		return H;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT InI) const
	{
		auto H = Textures->GetGPUDescriptorHandleForHeapStart();
		H.ptr += std::uint64_t(InI) * TextureStep;
		return H;
	}
};
} // namespace Hyperion
