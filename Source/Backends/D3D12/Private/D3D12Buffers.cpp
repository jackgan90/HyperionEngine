#include "D3D12RHIDevice.h"
#include "D3D12Resources.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace Hyperion
{
namespace
{
FBuffer CreateStorageBuffer(const std::shared_ptr<FD3D12DeviceState>& InState, const FBufferDesc& InDesc,
                            std::span<const std::byte> InBytes)
{
	if ((InDesc.Usage & 7U) != 0 || InDesc.Size % 4 != 0)
	{
		throw std::invalid_argument("Storage buffer requires exclusive shader usage and four-byte alignment");
	}
	auto Buffer =
	    InState->AllocateBuffer(InDesc.Size, D3D12_HEAP_TYPE_DEFAULT,
	                            InBytes.empty() ? Native(EResourceState::ShaderRead) : D3D12_RESOURCE_STATE_COPY_DEST,
	                            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	Buffer->Usage = InDesc.Usage;
	if (!InBytes.empty())
	{
		auto Upload = InState->AllocateBuffer(InDesc.Size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* Mapped{};
		const D3D12_RANGE Read{0, 0};
		Check(Upload->Resource->Map(0, &Read, &Mapped), "Map storage buffer upload");
		std::memset(Mapped, 0, static_cast<std::size_t>(InDesc.Size));
		std::memcpy(Mapped, InBytes.data(), InBytes.size());
		Upload->Resource->Unmap(0, nullptr);
		FD3D12DeviceState::FUploadBatch Batch;
		Batch.Resources = {Upload->Resource, Buffer->Resource};
		Batch.Allocations = {Upload->Allocation, Buffer->Allocation};
		Check(InState->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Batch.Allocator)),
		      "Storage upload allocator");
		Check(InState->Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Batch.Allocator.Get(), nullptr,
		                                         IID_PPV_ARGS(&Batch.List)),
		      "Storage upload list");
		Batch.List->CopyBufferRegion(Buffer->Resource.Get(), 0, Upload->Resource.Get(), 0, InDesc.Size);
		Transition(Batch.List.Get(), Buffer->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		           Native(EResourceState::ShaderRead));
		Check(Batch.List->Close(), "Close storage upload");
		InState->Uploads.push_back(std::move(Batch));
		auto& Submitted = InState->Uploads.back();
		ID3D12CommandList* Lists[]{Submitted.List.Get()};
		InState->Queue->ExecuteCommandLists(1, Lists);
		Submitted.FenceValue = InState->Signal();
		Buffer->UploadFence = Submitted.FenceValue;
	}
	return {std::move(Buffer)};
}
} // namespace

FBuffer FD3D12RHIDevice::CreateBuffer(const FBufferDesc& InDesc, std::span<const std::byte> InBytes)
{
	HYP_PERF_SCOPE_C(Detail, CreateBuffer);
	if (InDesc.Size == 0 || InDesc.Size > std::numeric_limits<std::size_t>::max() || InDesc.Usage == 0 ||
	    (InDesc.Usage & ~127U) != 0 || InBytes.size() > InDesc.Size)
	{
		throw std::invalid_argument("Invalid typed buffer size, usage or initial data");
	}
	if ((InDesc.Usage & (BufferUsage(ERHIBufferUsage::StructuredWrite) | BufferUsage(ERHIBufferUsage::RawWrite))) != 0)
	{
		return CreateStorageBuffer(State, InDesc, InBytes);
	}
	const bool bConstant = (InDesc.Usage & BufferUsage(ERHIBufferUsage::Constant)) != 0;
	if (bConstant && (InDesc.Usage != BufferUsage(ERHIBufferUsage::Constant) || InDesc.Size % 256 != 0))
	{
		throw std::invalid_argument("Constant pages require exclusive constant usage and 256-byte alignment");
	}
	auto Buffer = State->AllocateBuffer(InDesc.Size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	Buffer->Usage = InDesc.Usage;
	if ((InDesc.Usage & BufferUsage(ERHIBufferUsage::Index)) != 0)
	{
		Buffer->IndexData.resize(static_cast<std::size_t>(InDesc.Size / sizeof(std::uint32_t)));
		const auto Bytes = std::min(InBytes.size(), Buffer->IndexData.size() * sizeof(std::uint32_t));
		if (Bytes != 0)
		{
			std::memcpy(Buffer->IndexData.data(), InBytes.data(), Bytes);
		}
	}
	void* Mapped{};
	D3D12_RANGE Read{0, 0};
	Check(Buffer->Resource->Map(0, &Read, &Mapped), "Map typed upload buffer");
	std::memset(Mapped, 0, static_cast<std::size_t>(InDesc.Size));
	if (!InBytes.empty())
	{
		std::memcpy(Mapped, InBytes.data(), InBytes.size());
	}
	Buffer->Resource->Unmap(0, nullptr);
	return {std::move(Buffer)};
}

FBufferSlice FD3D12RHIDevice::PublishConstantSlice(const FBuffer& InBuffer, std::uint64_t InOffset,
                                                   std::span<const std::byte> InBytes)
{
	HYP_PERF_SCOPE_C(Detail, UploadConstantSlice);
	NativeResource<FD3D12Buffer>(InBuffer.Payload, State.get());
	auto& Buffer = *static_cast<FD3D12Buffer*>(InBuffer.Payload.get());
	std::lock_guard Lock(Buffer.ConstantMutex);
	if (Buffer.Usage != BufferUsage(ERHIBufferUsage::Constant) || InBytes.empty() || InBytes.size() > 65536 ||
	    InOffset % 256 != 0 || InOffset < Buffer.PublishedEnd || InOffset > Buffer.Size)
	{
		throw std::invalid_argument("Invalid constant slice usage, alignment, size or published overlap");
	}
	const std::uint32_t Size = static_cast<std::uint32_t>(InBytes.size());
	const std::uint32_t Extent = (Size + 255U) & ~255U;
	if (Extent > Buffer.Size - InOffset || Buffer.NextPublication == UINT64_MAX)
	{
		throw std::invalid_argument("Constant slice exceeds page or publication identity limit");
	}
	// Reserve metadata before touching bytes, so allocation failure cannot partially publish a slice.
	if (Buffer.Published.size() == Buffer.Published.capacity())
	{
		Buffer.Published.reserve(std::max<std::size_t>(8, Buffer.Published.size() * 2));
	}
	void* Mapped{};
	D3D12_RANGE Read{0, 0};
	Check(Buffer.Resource->Map(0, &Read, &Mapped), "Map constant page");
	auto* Destination = static_cast<std::byte*>(Mapped) + InOffset;
	std::memset(Destination, 0, Extent);
	std::memcpy(Destination, InBytes.data(), Size);
	const D3D12_RANGE Written{static_cast<SIZE_T>(InOffset), static_cast<SIZE_T>(InOffset + Extent)};
	Buffer.Resource->Unmap(0, &Written);
	const std::uint64_t Publication = Buffer.NextPublication++;
	Buffer.Published.push_back({InOffset, Size, Extent, Publication});
	Buffer.PublishedEnd = InOffset + Extent;
	State->ConstantBytesWritten += Size;
	return {InBuffer, InOffset, Size, Extent, Publication};
}

void FD3D12RHIDevice::ResetConstantBuffer(const FBuffer& InBuffer)
{
	NativeResource<FD3D12Buffer>(InBuffer.Payload, State.get());
	auto& Buffer = *static_cast<FD3D12Buffer*>(InBuffer.Payload.get());
	std::lock_guard Lock(Buffer.ConstantMutex);
	std::erase_if(Buffer.ConstantOwners,
	              [](const auto& InOwner)
	              {
		              return InOwner.expired();
	              });
	if (Buffer.Usage != BufferUsage(ERHIBufferUsage::Constant) || InBuffer.Payload.use_count() != 1 ||
	    !Buffer.ConstantOwners.empty())
	{
		throw std::logic_error("Constant page reset requires sole ownership after all slice/GPU users finish");
	}
	Buffer.Published.clear();
	Buffer.PublishedEnd = 0;
}
} // namespace Hyperion
