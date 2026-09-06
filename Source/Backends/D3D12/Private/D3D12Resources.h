#pragma once
#include "D3D12DeviceState.h"

namespace Hyperion
{
struct FD3D12Buffer final : IRHIBuffer
{
	const void* GetDeviceIdentity() const noexcept override
	{
		return State.get();
	}

	std::shared_ptr<FD3D12DeviceState> State;
	ComPtr<D3D12MA::Allocation> Allocation;
	ComPtr<ID3D12Resource> Resource;
	std::uint64_t Size{};
};

struct FD3D12Texture final : IRHITexture
{
	const void* GetDeviceIdentity() const noexcept override
	{
		return State.get();
	}

	std::shared_ptr<FD3D12DeviceState> State;
	ComPtr<D3D12MA::Allocation> Allocation;
	ComPtr<ID3D12Resource> Resource;
	UINT Slot = TextureCount;
	std::uint64_t UploadFence{};

	~FD3D12Texture() override
	{
		if (Slot < TextureCount)
		{
			State->Release(Slot);
		}
	}
};

struct FD3D12Pipeline final : IRHIPipeline
{
	std::shared_ptr<FD3D12DeviceState> State;

	const void* GetDeviceIdentity() const noexcept override
	{
		return State.get();
	}

	ComPtr<ID3D12RootSignature> Root;
	ComPtr<ID3D12PipelineState> Pipeline;
	bool Textured{};
	bool MaterialLayout{};
};

struct FD3D12SwapchainIdentity
{
};

struct FD3D12RecordedList final : IRHIRecordedList
{
	std::shared_ptr<FD3D12DeviceState> State;

	const void* GetDeviceIdentity() const noexcept override
	{
		return State.get();
	}

	ComPtr<ID3D12GraphicsCommandList> List;
	std::vector<FDrawPacket> Retained;
	std::uint64_t Frame{};
	UINT Context{};
	std::shared_ptr<const FD3D12SwapchainIdentity> Owner;
};

template<typename TResource, typename TInterface>
const TResource& NativeResource(const std::shared_ptr<TInterface>& InPayload, const FD3D12DeviceState* InOwner)
{
	const auto* Resource = dynamic_cast<const TResource*>(InPayload.get());
	if (!Resource || Resource->GetDeviceIdentity() != InOwner)
	{
		throw std::invalid_argument("Empty or foreign RHI resource (backend/device mismatch)");
	}
	return *Resource;
}
} // namespace Hyperion
