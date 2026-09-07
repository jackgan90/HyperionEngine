#include "D3D12RHIDevice.h"
#include "D3D12Resources.h"
#include <cstring>
#include <limits>

namespace Hyperion
{
FBuffer FD3D12RHIDevice::CreateBuffer(const FBufferDesc& InDesc, std::span<const std::byte> InBytes)
{
	if (InDesc.Size == 0 || InDesc.Size > std::numeric_limits<std::size_t>::max() || InDesc.Usage == 0 ||
	    (InDesc.Usage & ~31U) != 0 || InBytes.size() > InDesc.Size)
	{
		throw std::invalid_argument("Invalid typed buffer size, usage or initial data");
	}
	const bool bConstant = (InDesc.Usage & BufferUsage(ERHIBufferUsage::Constant)) != 0;
	if (bConstant && (InDesc.Usage != BufferUsage(ERHIBufferUsage::Constant) || InDesc.Size % 256 != 0))
	{
		throw std::invalid_argument("Constant pages require exclusive constant usage and 256-byte alignment");
	}
	auto Buffer = State->AllocateBuffer(InDesc.Size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	Buffer->Usage = InDesc.Usage;
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
	NativeResource<FD3D12Buffer>(InBuffer.Payload, State.get());
	auto& Buffer = *static_cast<FD3D12Buffer*>(InBuffer.Payload.get());
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
	Buffer.Published.reserve(Buffer.Published.size() + 1);
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
	if (Buffer.Usage != BufferUsage(ERHIBufferUsage::Constant) || InBuffer.Payload.use_count() != 1)
	{
		throw std::logic_error("Constant page reset requires sole ownership after all slice/GPU users finish");
	}
	Buffer.Published.clear();
	Buffer.PublishedEnd = 0;
}
} // namespace Hyperion
