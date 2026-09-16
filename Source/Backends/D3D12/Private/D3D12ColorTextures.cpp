#include "D3D12GraphicsState.h"
#include "D3D12RHIDevice.h"
#include "D3D12Resources.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
void InitializeColor(FD3D12DeviceState& InState, const FD3D12Texture& InTexture, const FVec4& InClear)
{
	FD3D12DeviceState::FUploadBatch Batch;
	Batch.Resources.push_back(InTexture.Resource);
	Batch.Allocations.push_back(InTexture.Allocation);
	Batch.DescriptorHeaps.push_back(InTexture.ColorViews);
	Check(InState.Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Batch.Allocator)),
	      "Color initialization allocator");
	Check(InState.Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Batch.Allocator.Get(), nullptr,
	                                        IID_PPV_ARGS(&Batch.List)),
	      "Color initialization list");
	const float Color[]{InClear.X, InClear.Y, InClear.Z, InClear.W};
	Batch.List->ClearRenderTargetView(InTexture.ColorViews->GetCPUDescriptorHandleForHeapStart(), Color, 0, nullptr);
	Transition(Batch.List.Get(), InTexture.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
	           Native(EResourceState::ShaderRead));
	Check(Batch.List->Close(), "Close color initialization");
	InState.Uploads.push_back(std::move(Batch));
	auto& Submitted = InState.Uploads.back();
	ID3D12CommandList* Lists[]{Submitted.List.Get()};
	InState.Queue->ExecuteCommandLists(1, Lists);
	Submitted.FenceValue = InState.Signal();
}
} // namespace

FTexture FD3D12RHIDevice::CreateColorTexture(const FColorTextureDesc& InDesc)
{
	const std::array ClearValues{InDesc.Clear.X, InDesc.Clear.Y, InDesc.Clear.Z, InDesc.Clear.W};
	if (InDesc.Format >= ERHIColorFormat::Count ||
	    !State->Capabilities.SampledColorTargets[static_cast<std::size_t>(InDesc.Format)] || !InDesc.Width ||
	    !InDesc.Height || InDesc.Width > State->Capabilities.MaxTextureDimension ||
	    InDesc.Height > State->Capabilities.MaxTextureDimension ||
	    !std::all_of(ClearValues.begin(), ClearValues.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }))
	{
		throw std::invalid_argument("Invalid or unsupported sampled color target");
	}
	auto Texture = std::make_shared<FD3D12Texture>();
	Texture->State = State;
	Texture->ColorFormat = InDesc.Format;
	D3D12_RESOURCE_DESC Desc{};
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	Desc.Width = InDesc.Width;
	Desc.Height = InDesc.Height;
	Desc.DepthOrArraySize = 1;
	Desc.MipLevels = 1;
	const auto ViewFormat = NativeColorFormat(InDesc.Format);
	const bool bDualViews = InDesc.Format == ERHIColorFormat::Rgba8Unorm;
	Desc.Format = bDualViews ? DXGI_FORMAT_R8G8B8A8_TYPELESS : ViewFormat;
	Desc.SampleDesc.Count = 1;
	Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE Clear{};
	Clear.Format = ViewFormat;
	std::copy(ClearValues.begin(), ClearValues.end(), Clear.Color);
	D3D12MA::ALLOCATION_DESC Allocation{};
	Allocation.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	Check(State->Allocator->CreateResource(&Allocation, &Desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &Clear,
	                                       &Texture->Allocation, IID_PPV_ARGS(&Texture->Resource)),
	      "Create sampled color target");
	D3D12_DESCRIPTOR_HEAP_DESC Heap{};
	Heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	Heap.NumDescriptors = bDualViews ? 2 : 1;
	Check(State->Device->CreateDescriptorHeap(&Heap, IID_PPV_ARGS(&Texture->ColorViews)), "Create color view heap");
	D3D12_RENDER_TARGET_VIEW_DESC View{};
	View.Format = ViewFormat;
	View.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	auto Handle = Texture->ColorViews->GetCPUDescriptorHandleForHeapStart();
	State->Device->CreateRenderTargetView(Texture->Resource.Get(), &View, Handle);
	if (bDualViews)
	{
		Handle.ptr += State->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		View.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		State->Device->CreateRenderTargetView(Texture->Resource.Get(), &View, Handle);
	}
	Texture->SourceDescriptor = State->ResourceSources.Reserve(1);
	D3D12_SHADER_RESOURCE_VIEW_DESC Srv{};
	Srv.Format = ViewFormat;
	Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Srv.Texture2D.MipLevels = 1;
	State->Device->CreateShaderResourceView(Texture->Resource.Get(), &Srv,
	                                        State->ResourceSources.Cpu(Texture->SourceDescriptor.Offset));
	State->DescriptorAllocations += Heap.NumDescriptors + 1;
	InitializeColor(*State, *Texture, InDesc.Clear);
	return {std::move(Texture)};
}
} // namespace Hyperion
