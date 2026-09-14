#include "D3D12GraphicsState.h"
#include "D3D12RHIDevice.h"
#include "D3D12Resources.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include <algorithm>
#include <bit>

namespace Hyperion
{
FPipeline FD3D12RHIDevice::CreateComputePipeline(const FComputePipelineDesc& InDesc)
{
	ValidateComputePipeline(InDesc, State->Capabilities);
	const auto& Layout = NativeResource<FD3D12BindingLayout>(InDesc.Layout.Payload, State.get());
	ValidatePipelineBindings(InDesc, Layout.Description);
	auto Pipeline = std::make_shared<FD3D12Pipeline>();
	Pipeline->State = State;
	Pipeline->Layout = InDesc.Layout;
	Pipeline->Root = Layout.Root;
	Pipeline->bCompute = true;
	D3D12_COMPUTE_PIPELINE_STATE_DESC Desc{};
	Desc.pRootSignature = Layout.Root.Get();
	Desc.CS = {InDesc.Compute.Bytes.data(), InDesc.Compute.Bytes.size()};
	Check(State->Device->CreateComputePipelineState(&Desc, IID_PPV_ARGS(&Pipeline->Pipeline)),
	      "Create compute pipeline");
	++State->PipelinesCreated;
	return {std::move(Pipeline)};
}

FTexture FD3D12RHIDevice::CreateStorageTexture(const FStorageTextureDesc& InDesc)
{
	if (!InDesc.Width || !InDesc.Height || InDesc.Width > State->Capabilities.MaxTextureDimension ||
	    InDesc.Height > State->Capabilities.MaxTextureDimension || !InDesc.MipCount ||
	    InDesc.MipCount > static_cast<std::uint32_t>(std::bit_width(std::max(InDesc.Width, InDesc.Height))) ||
	    InDesc.Format >= ERHIColorFormat::Count ||
	    !State->Capabilities.StorageTextures[static_cast<std::size_t>(InDesc.Format)])
	{
		throw std::invalid_argument("Invalid or unsupported storage texture size, mips or format");
	}
	auto Texture = std::make_shared<FD3D12Texture>();
	Texture->State = State;
	Texture->ColorFormat = InDesc.Format;
	D3D12_RESOURCE_DESC Desc{};
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	Desc.Width = InDesc.Width;
	Desc.Height = InDesc.Height;
	Desc.DepthOrArraySize = 1;
	Desc.MipLevels = static_cast<UINT16>(InDesc.MipCount);
	Desc.Format = NativeColorFormat(InDesc.Format);
	Desc.SampleDesc.Count = 1;
	Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12MA::ALLOCATION_DESC Allocation{};
	Allocation.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	Check(State->Allocator->CreateResource(&Allocation, &Desc, Native(EResourceState::ShaderRead), nullptr,
	                                       &Texture->Allocation, IID_PPV_ARGS(&Texture->Resource)),
	      "Create storage texture");
	Texture->SourceDescriptor = State->ResourceSources.Reserve(1);
	D3D12_SHADER_RESOURCE_VIEW_DESC Srv{};
	Srv.Format = Desc.Format;
	Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Srv.Texture2D.MipLevels = InDesc.MipCount;
	State->Device->CreateShaderResourceView(Texture->Resource.Get(), &Srv,
	                                        State->ResourceSources.Cpu(Texture->SourceDescriptor.Offset));
	++State->DescriptorAllocations;
	return {std::move(Texture)};
}
} // namespace Hyperion
