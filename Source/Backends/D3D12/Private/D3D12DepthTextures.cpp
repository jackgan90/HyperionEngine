#include "D3D12RHIDevice.h"
#include "D3D12Resources.h"
#include <cmath>

namespace Hyperion
{
namespace
{
void InitializeDepth(FD3D12DeviceState& InState, const FD3D12Texture& InTexture, float InClearDepth)
{
	FD3D12DeviceState::FUploadBatch Batch;
	Batch.Resources.push_back(InTexture.Resource);
	Batch.Allocations.push_back(InTexture.Allocation);
	Batch.DescriptorHeaps.push_back(InTexture.DepthViews);
	Check(InState.Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Batch.Allocator)),
	      "Depth initialization allocator");
	Check(InState.Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Batch.Allocator.Get(), nullptr,
	                                        IID_PPV_ARGS(&Batch.List)),
	      "Depth initialization list");
	Batch.List->ClearDepthStencilView(InTexture.DepthViews->GetCPUDescriptorHandleForHeapStart(),
	                                  D3D12_CLEAR_FLAG_DEPTH, InClearDepth, 0, 0, nullptr);
	Transition(Batch.List.Get(), InTexture.Resource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
	           Native(EResourceState::ShaderRead));
	Check(Batch.List->Close(), "Close depth initialization");
	InState.Uploads.push_back(std::move(Batch));
	auto& Submitted = InState.Uploads.back();
	ID3D12CommandList* Lists[]{Submitted.List.Get()};
	InState.Queue->ExecuteCommandLists(1, Lists);
	Submitted.FenceValue = InState.Signal();
	// The same graphics queue executes all later consumers; no CPU wait or staging upload is needed.
}
} // namespace

FTexture FD3D12RHIDevice::CreateDepthTexture(const FDepthTextureDesc& InDesc)
{
	if (!State->Capabilities.bSampledDepthTargets || !InDesc.Width || !InDesc.Height ||
	    InDesc.Width > State->Capabilities.MaxTextureDimension ||
	    InDesc.Height > State->Capabilities.MaxTextureDimension || !std::isfinite(InDesc.ClearDepth) ||
	    InDesc.ClearDepth < 0 || InDesc.ClearDepth > 1)
	{
		throw std::invalid_argument("Invalid or unsupported sampled depth texture");
	}
	auto Texture = std::make_shared<FD3D12Texture>();
	Texture->State = State;
	Texture->DepthSize = {InDesc.Width, InDesc.Height};
	D3D12_RESOURCE_DESC Desc{};
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	Desc.Width = InDesc.Width;
	Desc.Height = InDesc.Height;
	Desc.DepthOrArraySize = 1;
	Desc.MipLevels = 1;
	Desc.Format = DXGI_FORMAT_R32_TYPELESS;
	Desc.SampleDesc.Count = 1;
	Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE Clear{};
	Clear.Format = DXGI_FORMAT_D32_FLOAT;
	Clear.DepthStencil.Depth = InDesc.ClearDepth;
	D3D12MA::ALLOCATION_DESC Allocation{};
	Allocation.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	Check(State->Allocator->CreateResource(&Allocation, &Desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &Clear,
	                                       &Texture->Allocation, IID_PPV_ARGS(&Texture->Resource)),
	      "Create sampled depth");
	D3D12_DESCRIPTOR_HEAP_DESC Heap{};
	Heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	Heap.NumDescriptors = 1;
	Check(State->Device->CreateDescriptorHeap(&Heap, IID_PPV_ARGS(&Texture->DepthViews)), "Create depth view heap");
	D3D12_DEPTH_STENCIL_VIEW_DESC Dsv{};
	Dsv.Format = DXGI_FORMAT_D32_FLOAT;
	Dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	State->Device->CreateDepthStencilView(Texture->Resource.Get(), &Dsv,
	                                      Texture->DepthViews->GetCPUDescriptorHandleForHeapStart());
	Texture->SourceDescriptor = State->ResourceSources.Reserve(1);
	D3D12_SHADER_RESOURCE_VIEW_DESC Srv{};
	Srv.Format = DXGI_FORMAT_R32_FLOAT;
	Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Srv.Texture2D.MipLevels = 1;
	State->Device->CreateShaderResourceView(Texture->Resource.Get(), &Srv,
	                                        State->ResourceSources.Cpu(Texture->SourceDescriptor.Offset));
	State->DescriptorAllocations += 2;
	InitializeDepth(*State, *Texture, InDesc.ClearDepth);
	return {std::move(Texture)};
}
} // namespace Hyperion
