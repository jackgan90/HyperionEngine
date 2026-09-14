#include "D3D12Dispatch.h"
#include "D3D12Bindings.h"
#include <algorithm>

namespace Hyperion
{
void ValidateTextureAccess(const FTextureView& InView, EResourceState InState, const FPassCommands& InCommands)
{
	if (!InView.Texture || !InView.MipCount ||
	    std::uint64_t(InView.FirstMip) + InView.MipCount > InView.Texture.Payload->GetInfo().MipCount)
	{
		throw std::invalid_argument("Invalid declared texture view");
	}
	for (std::uint32_t Mip = InView.FirstMip; Mip < InView.FirstMip + InView.MipCount; ++Mip)
	{
		const bool bCovered =
		    std::any_of(InCommands.TextureAccesses.begin(), InCommands.TextureAccesses.end(),
		                [&](const auto& InAccess)
		                {
			                return InAccess.View.Texture == InView.Texture && InAccess.State == InState &&
			                       InAccess.View.FirstMip <= Mip &&
			                       std::uint64_t(InAccess.View.FirstMip) + InAccess.View.MipCount > Mip;
		                });
		if (!bCovered)
		{
			throw std::invalid_argument("Dispatch texture binding is not covered by declared mip access");
		}
	}
}

void ValidateBufferAccess(const FReadBufferView& InView, EResourceState InState, const FPassCommands& InCommands)
{
	const bool bCovered =
	    std::any_of(InCommands.BufferAccesses.begin(), InCommands.BufferAccesses.end(),
	                [&](const auto& InAccess)
	                {
		                return InAccess.View.Buffer == InView.Buffer && InAccess.State == InState &&
		                       InAccess.View.Offset <= InView.Offset && InView.Size <= InAccess.View.Size &&
		                       InView.Offset - InAccess.View.Offset <= InAccess.View.Size - InView.Size;
	                });
	if (!bCovered)
	{
		throw std::invalid_argument("Buffer binding is not covered by declared range access");
	}
}

namespace
{
void ValidateAccesses(const FD3D12BindingSet& InSet, const FD3D12BindingLayout& InLayout,
                      const FPassCommands& InCommands)
{
	for (const auto& Entry : InSet.Description.Entries)
	{
		const auto State = IsStorageBinding(InLayout.Description.Slots[Entry.Slot].Kind) ? EResourceState::ShaderWrite
		                                                                                 : EResourceState::ShaderRead;
		for (const auto& Value : Entry.Values)
		{
			if (const auto* View = std::get_if<FTextureView>(&Value))
			{
				ValidateTextureAccess(*View, State, InCommands);
			}
			else if (const auto* Texture = std::get_if<FTexture>(&Value))
			{
				ValidateTextureAccess({*Texture, 0, Texture->Payload->GetInfo().MipCount}, State, InCommands);
			}
			else if (const auto* BufferView = std::get_if<FReadBufferView>(&Value))
			{
				ValidateBufferAccess(*BufferView, State, InCommands);
			}
		}
	}
}
} // namespace

void ValidateDispatches(const FD3D12DeviceState& InState, const std::shared_ptr<const FPassCommands>& InOwnedCommands)
{
	const auto& InCommands = *InOwnedCommands;
	if (!InCommands.bCompute && !InCommands.Dispatches.empty())
	{
		throw std::invalid_argument("Dispatch requires compute pass");
	}
	for (const auto& Dispatch : InCommands.Dispatches)
	{
		const auto& Pipeline = NativeResource<FD3D12Pipeline>(Dispatch.Pipeline.Payload, &InState);
		if (!Pipeline.bCompute)
		{
			throw std::invalid_argument("Dispatch requires compute pipeline");
		}
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			if (!Dispatch.Groups[Axis] || Dispatch.Groups[Axis] > InState.Capabilities.MaxDispatchGroups[Axis])
			{
				throw std::invalid_argument("Dispatch group count exceeds device limits");
			}
		}
		RetainShaderConstantPages(InState, Dispatch.ConstantBindings, InOwnedCommands);
		ValidateShaderBindings(Dispatch.Bindings, Dispatch.ConstantBindings, 1, Pipeline, InState,
		                       InState.Fence->GetCompletedValue());
		if (Dispatch.Bindings)
		{
			ValidateAccesses(NativeResource<FD3D12BindingSet>(Dispatch.Bindings.Payload, &InState),
			                 NativeResource<FD3D12BindingLayout>(Pipeline.Layout.Payload, &InState), InCommands);
		}
	}
}

void RecordDispatches(ID3D12GraphicsCommandList& InList, const FD3D12DeviceState& InState,
                      const FPassCommands& InCommands)
{
	bool bHaveDispatch = false;
	for (const auto& Dispatch : InCommands.Dispatches)
	{
		if (bHaveDispatch)
		{
			D3D12_RESOURCE_BARRIER Barrier{};
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			InList.ResourceBarrier(1, &Barrier);
		}
		bHaveDispatch = true;
		const auto& Pipeline = NativeResource<FD3D12Pipeline>(Dispatch.Pipeline.Payload, &InState);
		const auto& Layout = NativeResource<FD3D12BindingLayout>(Pipeline.Layout.Payload, &InState);
		InList.SetPipelineState(Pipeline.Pipeline.Get());
		InList.SetComputeRootSignature(Pipeline.Root.Get());
		ID3D12DescriptorHeap* Heaps[]{InState.ResourceTables.GetHeap(), InState.SamplerTables.GetHeap()};
		InList.SetDescriptorHeaps(2, Heaps);
		for (const auto& Constant : Dispatch.ConstantBindings)
		{
			const auto& Buffer = NativeResource<FD3D12Buffer>(Constant.Slice.Buffer.Payload, &InState);
			InList.SetComputeRootConstantBufferView(Layout.Slots[Constant.Slot].RootParameter,
			                                        Buffer.Resource->GetGPUVirtualAddress() + Constant.Slice.Offset);
		}
		if (Dispatch.Bindings)
		{
			const auto& Set = NativeResource<FD3D12BindingSet>(Dispatch.Bindings.Payload, &InState);
			for (std::size_t Index = 0; Index < Layout.Tables.size(); ++Index)
			{
				const auto& Arena = Layout.Tables[Index].bSampler ? InState.SamplerTables : InState.ResourceTables;
				InList.SetComputeRootDescriptorTable(Layout.Tables[Index].RootParameter,
				                                     Arena.Gpu(Set.Tables[Index].Offset));
			}
		}
		InList.Dispatch(Dispatch.Groups[0], Dispatch.Groups[1], Dispatch.Groups[2]);
	}
}
} // namespace Hyperion
