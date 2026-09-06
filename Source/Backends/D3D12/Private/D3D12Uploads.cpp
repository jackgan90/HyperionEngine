#include "D3D12RHIDevice.h"
#include "D3D12Resources.h"

namespace Hyperion
{
std::vector<FTexture> FD3D12RHIDevice::CreateTexturesAsync(std::span<const FTextureDesc> InTextures)
{
	auto& P = *State;
	P.CollectUploads();
	if (InTextures.empty())
	{
		return {};
	}
	FD3D12DeviceState::FUploadBatch Batch;
	Check(P.Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Batch.Allocator)),
	      "Create upload allocator");
	Check(P.Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Batch.Allocator.Get(), nullptr,
	                                  IID_PPV_ARGS(&Batch.List)),
	      "Create upload list");
	std::vector<FTexture> Textures;
	for (const auto& Source : InTextures)
	{
		if (Source.Mips.empty() || Source.Mips.size() > 15)
		{
			throw std::invalid_argument("Invalid texture mip count");
		}
		const auto& Base = Source.Mips.front();
		if (!Base.Width || !Base.Height || Base.Width > 16384 || Base.Height > 16384)
		{
			throw std::invalid_argument("Invalid texture dimensions");
		}
		std::uint32_t Width = Base.Width;
		std::uint32_t Height = Base.Height;
		for (const auto& Mip : Source.Mips)
		{
			if (Mip.Width != Width || Mip.Height != Height || Mip.Rgba.size() != std::size_t(Width) * Height * 4)
			{
				throw std::invalid_argument("Invalid mip dimensions or bytes");
			}
			Width = std::max(1u, Width / 2);
			Height = std::max(1u, Height / 2);
		}
		auto Texture = std::make_shared<FD3D12Texture>();
		Texture->State = State;
		D3D12_RESOURCE_DESC Desc{};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Width = Base.Width;
		Desc.Height = Base.Height;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = static_cast<UINT16>(Source.Mips.size());
		Desc.Format = Source.Srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		Desc.SampleDesc.Count = 1;
		D3D12MA::ALLOCATION_DESC Allocation{};
		Allocation.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		Check(P.Allocator->CreateResource(&Allocation, &Desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		                                  &Texture->Allocation, IID_PPV_ARGS(&Texture->Resource)),
		      "Allocate model texture");
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> Footprints(Source.Mips.size());
		UINT64 Total{};
		P.Device->GetCopyableFootprints(&Desc, 0, Desc.MipLevels, 0, Footprints.data(), nullptr, nullptr, &Total);
		auto Upload = P.AllocateBuffer(Total, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* Mapped{};
		D3D12_RANGE Read{0, 0};
		Check(Upload->Resource->Map(0, &Read, &Mapped), "Map model upload");
		for (UINT Index = 0; Index < Source.Mips.size(); ++Index)
		{
			const auto& Mip = Source.Mips[Index];
			const auto& Footprint = Footprints[Index];
			for (UINT Row = 0; Row < Mip.Height; ++Row)
			{
				std::memcpy(static_cast<std::byte*>(Mapped) + Footprint.Offset +
				                std::size_t(Row) * Footprint.Footprint.RowPitch,
				            Mip.Rgba.data() + std::size_t(Row) * Mip.Width * 4, std::size_t(Mip.Width) * 4);
			}
			D3D12_TEXTURE_COPY_LOCATION Destination{};
			Destination.pResource = Texture->Resource.Get();
			Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			Destination.SubresourceIndex = Index;
			D3D12_TEXTURE_COPY_LOCATION SourceLocation{};
			SourceLocation.pResource = Upload->Resource.Get();
			SourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			SourceLocation.PlacedFootprint = Footprint;
			Batch.List->CopyTextureRegion(&Destination, 0, 0, 0, &SourceLocation, nullptr);
		}
		Upload->Resource->Unmap(0, nullptr);
		Transition(Batch.List.Get(), Texture->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Texture->Slot = P.Reserve();
		D3D12_SHADER_RESOURCE_VIEW_DESC View{};
		View.Format = Desc.Format;
		View.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		View.Texture2D.MipLevels = Desc.MipLevels;
		P.Device->CreateShaderResourceView(Texture->Resource.Get(), &View, P.Cpu(Texture->Slot));
		Batch.Resources.push_back(Upload->Resource);
		Batch.Allocations.push_back(Upload->Allocation);
		Batch.Resources.push_back(Texture->Resource);
		Batch.Allocations.push_back(Texture->Allocation);
		Textures.push_back({std::move(Texture)});
	}
	Check(Batch.List->Close(), "Close model upload batch");
	P.Uploads.push_back(std::move(Batch));
	auto& Submitted = P.Uploads.back();
	ID3D12CommandList* Lists[] = {Submitted.List.Get()};
	P.Queue->ExecuteCommandLists(1, Lists);
	Submitted.FenceValue = P.Signal();
	for (auto& Texture : Textures)
	{
		static_cast<FD3D12Texture*>(Texture.Payload.get())->UploadFence = Submitted.FenceValue;
	}
	return Textures;
}

bool FD3D12RHIDevice::TexturesReady(std::span<const FTexture> InTextures)
{
	State->CollectUploads();
	const auto Completed = State->Fence->GetCompletedValue();
	for (const auto& Texture : InTextures)
	{
		if (NativeResource<FD3D12Texture>(Texture.Payload, State.get()).UploadFence > Completed)
		{
			return false;
		}
	}
	return true;
}
} // namespace Hyperion
