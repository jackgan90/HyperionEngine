#pragma once
#include "D3D12DeviceState.h"
#include "D3D12Profiling.h"
#include "Hyperion/RHI/RHISwapchain.h"

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
	std::uint32_t Usage = BufferUsage(ERHIBufferUsage::Vertex) | BufferUsage(ERHIBufferUsage::Index);
	std::uint64_t PublishedEnd{};
	std::uint64_t NextPublication = 1;

	struct FPublishedSlice
	{
		std::uint64_t Offset{};
		std::uint32_t Size{};
		std::uint32_t Extent{};
		std::uint64_t Publication{};
	};

	std::vector<FPublishedSlice> Published;
	// Upload heaps are write-combined. Validate immutable indices from ordinary CPU memory.
	std::vector<std::uint32_t> IndexData;
	// Immutable index data: cache bounded range min/max results across repeated draws and recording contexts.
	mutable std::mutex IndexRangesMutex;
	mutable std::vector<std::array<std::uint32_t, 4>> IndexRanges; // first, count, minimum, maximum.
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
	std::uint64_t UploadFence{};
	FD3D12DescriptorRange SourceDescriptor;
	ComPtr<ID3D12DescriptorHeap> DepthViews;
	FSize DepthSize;

	~FD3D12Texture() override
	{
		State->ResourceSources.Release(SourceDescriptor);
	}
};

struct FD3D12Sampler final : IRHISampler
{
	std::shared_ptr<FD3D12DeviceState> State;
	FSamplerDesc Description;
	FD3D12DescriptorRange SourceDescriptor;

	const void* GetDeviceIdentity() const noexcept override
	{
		return State.get();
	}

	~FD3D12Sampler() override
	{
		State->SamplerSources.Release(SourceDescriptor);
	}
};

struct FD3D12BindingLayout final : IRHIResourceBindingLayout
{
	std::shared_ptr<FD3D12DeviceState> State;
	FResourceBindingLayoutDesc Description;
	ComPtr<ID3D12RootSignature> Root;

	struct FSlot
	{
		UINT RootParameter{};
		UINT Table = UINT_MAX;
		UINT Offset{};
	};

	struct FTable
	{
		UINT RootParameter{};
		bool bSampler{};
		ERHIShaderVisibility Visibility{};
		UINT Count{};
		std::vector<D3D12_DESCRIPTOR_RANGE> Ranges;
	};

	std::vector<FSlot> Slots;
	std::vector<FTable> Tables;

	const void* GetDeviceIdentity() const noexcept override
	{
		return State.get();
	}
};

struct FD3D12BindingSet final : IRHIResourceBindingSet
{
	std::shared_ptr<FD3D12DeviceState> State;
	FResourceBindingSetDesc Description;
	std::vector<FD3D12DescriptorRange> Tables;

	const void* GetDeviceIdentity() const noexcept override
	{
		return State.get();
	}

	~FD3D12BindingSet() override;
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
	FResourceBindingLayout Layout;
	FGraphicsTarget Target;
	FGraphicsState GraphicsState;
	ERHIPrimitiveTopology Topology = ERHIPrimitiveTopology::TriangleList;
	std::uint32_t VertexStride{};
};

struct FD3D12SwapchainIdentity
{
};

struct FD3D12PassQueries;

struct FD3D12RecordedList final : IRHIRecordedList
{
	std::shared_ptr<FD3D12PassQueries> TimingQueries;
	std::string Name;
#if HYP_ENABLE_PROFILING
	std::shared_ptr<FD3D12ProfileQueries> ProfileQueries;
	FProfileGpuContext ProfileContext;
	FProfileGpuSpan ProfileSpan;
#endif
	std::shared_ptr<FD3D12DeviceState> State;

	const void* GetDeviceIdentity() const noexcept override
	{
		return State.get();
	}

	ComPtr<ID3D12GraphicsCommandList> List;
	std::vector<FDrawPacket> Retained;
	std::vector<FTexture> Textures;
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
