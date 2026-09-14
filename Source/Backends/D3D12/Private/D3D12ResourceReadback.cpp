#include "D3D12RHIDevice.h"
#include "D3D12Resources.h"
#include <cstring>

namespace Hyperion
{
std::vector<std::byte> FD3D12RHIDevice::ReadTexture(const FTextureView& InView, EResourceState InState)
{
	const auto& Texture = NativeResource<FD3D12Texture>(InView.Texture.Payload, State.get());
	if (InView.MipCount != 1 || InView.FirstMip >= Texture.GetInfo().MipCount ||
	    Texture.Dimension != ERHITextureDimension::Texture2D)
	{
		throw std::invalid_argument("Readback requires one valid 2D mip");
	}
	const auto Desc = Texture.Resource->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint{};
	UINT Rows{};
	UINT64 RowBytes{};
	UINT64 Total{};
	State->Device->GetCopyableFootprints(&Desc, InView.FirstMip, 1, 0, &Footprint, &Rows, &RowBytes, &Total);
	auto Readback = State->AllocateBuffer(Total, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	State->Immediate(
	    [&](ID3D12GraphicsCommandList* InList)
	    {
		    Transition(InList, Texture.Resource.Get(), Native(InState), D3D12_RESOURCE_STATE_COPY_SOURCE,
		               InView.FirstMip);
		    D3D12_TEXTURE_COPY_LOCATION Source{};
		    Source.pResource = Texture.Resource.Get();
		    Source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		    Source.SubresourceIndex = InView.FirstMip;
		    D3D12_TEXTURE_COPY_LOCATION Destination{};
		    Destination.pResource = Readback->Resource.Get();
		    Destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		    Destination.PlacedFootprint = Footprint;
		    InList->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);
		    Transition(InList, Texture.Resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, Native(InState),
		               InView.FirstMip);
	    },
	    {Texture.Resource, Readback->Resource}, {Texture.Allocation, Readback->Allocation});
	std::vector<std::byte> Result(static_cast<std::size_t>(Rows * RowBytes));
	void* Mapped{};
	const D3D12_RANGE Range{0, static_cast<SIZE_T>(Total)};
	Check(Readback->Resource->Map(0, &Range, &Mapped), "Map texture readback");
	for (UINT Row = 0; Row < Rows; ++Row)
	{
		std::memcpy(Result.data() + Row * RowBytes,
		            static_cast<const std::byte*>(Mapped) + Footprint.Offset + Row * Footprint.Footprint.RowPitch,
		            static_cast<std::size_t>(RowBytes));
	}
	const D3D12_RANGE Written{0, 0};
	Readback->Resource->Unmap(0, &Written);
	return Result;
}

std::vector<std::byte> FD3D12RHIDevice::ReadBuffer(const FReadBufferView& InView, EResourceState InState)
{
	const auto& Buffer = NativeResource<FD3D12Buffer>(InView.Buffer.Payload, State.get());
	if (!InView.Size || InView.Offset > Buffer.Size || InView.Size > Buffer.Size - InView.Offset ||
	    (Buffer.Usage & 96U) == 0)
	{
		throw std::invalid_argument("Readback requires a valid storage buffer range");
	}
	auto Readback = State->AllocateBuffer(InView.Size, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	State->Immediate(
	    [&](ID3D12GraphicsCommandList* InList)
	    {
		    Transition(InList, Buffer.Resource.Get(), Native(InState), D3D12_RESOURCE_STATE_COPY_SOURCE);
		    InList->CopyBufferRegion(Readback->Resource.Get(), 0, Buffer.Resource.Get(), InView.Offset, InView.Size);
		    Transition(InList, Buffer.Resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, Native(InState));
	    },
	    {Buffer.Resource, Readback->Resource}, {Buffer.Allocation, Readback->Allocation});
	std::vector<std::byte> Result(static_cast<std::size_t>(InView.Size));
	void* Mapped{};
	const D3D12_RANGE Range{0, Result.size()};
	Check(Readback->Resource->Map(0, &Range, &Mapped), "Map buffer readback");
	std::memcpy(Result.data(), Mapped, Result.size());
	const D3D12_RANGE Written{0, 0};
	Readback->Resource->Unmap(0, &Written);
	return Result;
}
} // namespace Hyperion
