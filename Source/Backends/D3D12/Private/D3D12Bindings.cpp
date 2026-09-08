#include "D3D12Bindings.h"
#include "D3D12RHIDevice.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
D3D12_TEXTURE_ADDRESS_MODE Address(ERHIAddressMode InMode)
{
	switch (InMode)
	{
		case ERHIAddressMode::Repeat:
			return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case ERHIAddressMode::Clamp:
			return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case ERHIAddressMode::Mirror:
			return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case ERHIAddressMode::Border:
			return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		case ERHIAddressMode::MirrorOnce:
			return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		default:
			throw std::invalid_argument("Invalid sampler address mode");
	}
}

void ValidateBufferView(const FReadBufferView& InView, ERHIBindingKind InKind, const FD3D12DeviceState& InState)
{
	const FD3D12Buffer& Buffer = NativeResource<FD3D12Buffer>(InView.Buffer.Payload, &InState);
	const bool bStructured = InKind == ERHIBindingKind::StructuredBuffer;
	const ERHIBufferUsage RequiredUsage = bStructured ? ERHIBufferUsage::StructuredRead : ERHIBufferUsage::RawRead;
	const std::uint32_t ElementSize = bStructured ? InView.Stride : 4;
	if ((Buffer.Usage & BufferUsage(RequiredUsage)) == 0 || InView.Size == 0 || InView.Offset > Buffer.Size ||
	    InView.Size > Buffer.Size - InView.Offset ||
	    (bStructured ? InView.Kind != ERHIBufferViewKind::Structured : InView.Kind != ERHIBufferViewKind::Raw) ||
	    ElementSize == 0 || ElementSize > 2048 || ElementSize % 4 != 0 || InView.Offset % ElementSize != 0 ||
	    InView.Size % ElementSize != 0 || InView.Size / ElementSize > UINT_MAX || (!bStructured && InView.Stride != 0))
	{
		throw std::invalid_argument("Invalid read buffer usage, view kind, stride or range");
	}
}

void ValidateValue(const FResourceBindingValue& InValue, ERHIBindingKind InKind, const FD3D12DeviceState& InState)
{
	if (InKind == ERHIBindingKind::Texture2D && std::holds_alternative<FTexture>(InValue))
	{
		NativeResource<FD3D12Texture>(std::get<FTexture>(InValue).Payload, &InState);
	}
	else if (InKind == ERHIBindingKind::Sampler && std::holds_alternative<FSampler>(InValue))
	{
		NativeResource<FD3D12Sampler>(std::get<FSampler>(InValue).Payload, &InState);
	}
	else if ((InKind == ERHIBindingKind::StructuredBuffer || InKind == ERHIBindingKind::RawBuffer) &&
	         std::holds_alternative<FReadBufferView>(InValue))
	{
		ValidateBufferView(std::get<FReadBufferView>(InValue), InKind, InState);
	}
	else
	{
		throw std::invalid_argument("Resource binding kind/value mismatch");
	}
}

void ValidateSet(const FResourceBindingSetDesc& InDesc, const FD3D12BindingLayout& InLayout,
                 const FD3D12DeviceState& InState)
{
	std::vector<bool> Seen(InLayout.Slots.size());
	for (const FResourceBindingEntry& Entry : InDesc.Entries)
	{
		if (Entry.Slot >= Seen.size() || Seen[Entry.Slot])
		{
			throw std::invalid_argument("Invalid or duplicate resource binding slot");
		}
		Seen[Entry.Slot] = true;
		const FResourceBindingSlot& Slot = InLayout.Description.Slots[Entry.Slot];
		if (Slot.Kind == ERHIBindingKind::ConstantBuffer || Slot.Count != Entry.Values.size())
		{
			throw std::invalid_argument("Invalid resource binding count or dynamic constant slot");
		}
		for (const FResourceBindingValue& Value : Entry.Values)
		{
			ValidateValue(Value, Slot.Kind, InState);
			if (Slot.Kind == ERHIBindingKind::StructuredBuffer && Slot.StructureByteStride != 0 &&
			    std::get<FReadBufferView>(Value).Stride != Slot.StructureByteStride)
			{
				throw std::invalid_argument("Structured buffer view stride does not match its binding layout");
			}
		}
	}
	for (std::size_t Index = 0; Index < Seen.size(); ++Index)
	{
		if (!Seen[Index] && InLayout.Description.Slots[Index].Kind != ERHIBindingKind::ConstantBuffer)
		{
			throw std::invalid_argument("Missing required resource binding slot");
		}
	}
}

void WriteDescriptor(FD3D12DeviceState& InState, D3D12_CPU_DESCRIPTOR_HANDLE InDestination,
                     const FResourceBindingValue& InValue, ERHIBindingKind InKind)
{
	if (const auto* Texture = std::get_if<FTexture>(&InValue))
	{
		const FD3D12Texture& Native = NativeResource<FD3D12Texture>(Texture->Payload, &InState);
		InState.Device->CopyDescriptorsSimple(1, InDestination,
		                                      InState.ResourceSources.Cpu(Native.SourceDescriptor.Offset),
		                                      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	else if (const auto* Sampler = std::get_if<FSampler>(&InValue))
	{
		const FD3D12Sampler& Native = NativeResource<FD3D12Sampler>(Sampler->Payload, &InState);
		InState.Device->CopyDescriptorsSimple(1, InDestination,
		                                      InState.SamplerSources.Cpu(Native.SourceDescriptor.Offset),
		                                      D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}
	else
	{
		const FReadBufferView& View = std::get<FReadBufferView>(InValue);
		const FD3D12Buffer& Buffer = NativeResource<FD3D12Buffer>(View.Buffer.Payload, &InState);
		const bool bRaw = InKind == ERHIBindingKind::RawBuffer;
		const UINT ElementSize = bRaw ? 4 : View.Stride;
		D3D12_SHADER_RESOURCE_VIEW_DESC Desc{};
		Desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Desc.Format = bRaw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
		Desc.Buffer.FirstElement = View.Offset / ElementSize;
		Desc.Buffer.NumElements = static_cast<UINT>(View.Size / ElementSize);
		Desc.Buffer.StructureByteStride = bRaw ? 0 : View.Stride;
		Desc.Buffer.Flags = bRaw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;
		InState.Device->CreateShaderResourceView(Buffer.Resource.Get(), &Desc, InDestination);
	}
	++InState.DescriptorCopies;
}

void ValidateConstant(const FConstantBinding& InBinding, const FResourceBindingSlot& InSlot,
                      const FD3D12DeviceState& InState)
{
	const FBufferSlice& Slice = InBinding.Slice;
	const FD3D12Buffer& Buffer = NativeResource<FD3D12Buffer>(Slice.Buffer.Payload, &InState);
	if (Buffer.Usage != BufferUsage(ERHIBufferUsage::Constant) || Slice.Offset % 256 != 0 || Slice.Size == 0 ||
	    Slice.Size > 65536 || Slice.Size < InSlot.MinimumBufferSize || Slice.Extent != ((Slice.Size + 255U) & ~255U) ||
	    Slice.Offset > Buffer.Size || Slice.Extent > Buffer.Size - Slice.Offset)
	{
		throw std::invalid_argument("Invalid constant buffer slice alignment or range");
	}
	if (!std::any_of(Buffer.Published.begin(), Buffer.Published.end(),
	                 [&Slice](const FD3D12Buffer::FPublishedSlice& InPublished)
	                 {
		                 return InPublished.Offset == Slice.Offset && InPublished.Size == Slice.Size &&
		                        InPublished.Extent == Slice.Extent && InPublished.Publication == Slice.Publication;
	                 }))
	{
		throw std::invalid_argument("Unpublished or stale constant buffer slice");
	}
}
} // namespace

FSampler FD3D12RHIDevice::CreateSampler(const FSamplerDesc& InDesc)
{
	if (InDesc.bComparison)
	{
		throw std::invalid_argument("Comparison samplers are unsupported");
	}
	if (InDesc.MaxAnisotropy == 0 || InDesc.MaxAnisotropy > State->Capabilities.MaxAnisotropy ||
	    (InDesc.MaxAnisotropy > 1 && (!InDesc.bMinLinear || !InDesc.bMagLinear || !InDesc.bMipLinear)) ||
	    !std::isfinite(InDesc.MipLodBias) || InDesc.MipLodBias < -16 || InDesc.MipLodBias > 15.99F ||
	    !std::isfinite(InDesc.MinLod) || !std::isfinite(InDesc.MaxLod) || InDesc.MinLod > InDesc.MaxLod ||
	    (!InDesc.bMipmapped && InDesc.MinLod > 0) ||
	    !std::all_of(InDesc.BorderColor.begin(), InDesc.BorderColor.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }))
	{
		throw std::invalid_argument("Invalid sampler LOD, anisotropy or border color");
	}
	D3D12_SAMPLER_DESC Desc{};
	Desc.AddressU = Address(InDesc.U);
	Desc.AddressV = Address(InDesc.V);
	Desc.AddressW = Address(InDesc.W);
	Desc.Filter =
	    InDesc.MaxAnisotropy > 1
	        ? D3D12_FILTER_ANISOTROPIC
	        : D3D12_ENCODE_BASIC_FILTER(InDesc.bMinLinear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
	                                    InDesc.bMagLinear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
	                                    InDesc.bMipLinear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
	                                    D3D12_FILTER_REDUCTION_TYPE_STANDARD);
	Desc.MipLODBias = InDesc.MipLodBias;
	Desc.MaxAnisotropy = InDesc.MaxAnisotropy;
	Desc.MinLOD = InDesc.MinLod;
	Desc.MaxLOD = InDesc.bMipmapped ? InDesc.MaxLod : 0;
	Desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	std::copy(InDesc.BorderColor.begin(), InDesc.BorderColor.end(), Desc.BorderColor);
	auto Sampler = std::make_shared<FD3D12Sampler>();
	Sampler->State = State;
	Sampler->Description = InDesc;
	Sampler->SourceDescriptor = State->SamplerSources.Reserve(1);
	State->Device->CreateSampler(&Desc, State->SamplerSources.Cpu(Sampler->SourceDescriptor.Offset));
	++State->DescriptorAllocations;
	return {std::move(Sampler)};
}

FD3D12BindingSet::~FD3D12BindingSet()
{
	const auto* Layout = dynamic_cast<const FD3D12BindingLayout*>(Description.Layout.Payload.get());
	if (!Layout)
	{
		return;
	}
	for (std::size_t Index = 0; Index < Tables.size(); ++Index)
	{
		auto& Arena = Layout->Tables[Index].bSampler ? State->SamplerTables : State->ResourceTables;
		Arena.Release(Tables[Index]);
	}
}

FResourceBindingSet FD3D12RHIDevice::CreateBindingSet(const FResourceBindingSetDesc& InDesc)
{
	const FD3D12BindingLayout& Layout = NativeResource<FD3D12BindingLayout>(InDesc.Layout.Payload, State.get());
	ValidateSet(InDesc, Layout, *State);
	auto Set = std::make_shared<FD3D12BindingSet>();
	Set->State = State;
	Set->Description = InDesc;
	Set->Tables.resize(Layout.Tables.size());
	for (std::size_t Index = 0; Index < Layout.Tables.size(); ++Index)
	{
		const FD3D12BindingLayout::FTable& Table = Layout.Tables[Index];
		auto& Arena = Table.bSampler ? State->SamplerTables : State->ResourceTables;
		Set->Tables[Index] = Arena.Reserve(Table.Count);
		State->DescriptorAllocations += Table.Count;
	}
	for (const FResourceBindingEntry& Entry : InDesc.Entries)
	{
		const FD3D12BindingLayout::FSlot& Slot = Layout.Slots[Entry.Slot];
		const FD3D12BindingLayout::FTable& Table = Layout.Tables[Slot.Table];
		auto& Arena = Table.bSampler ? State->SamplerTables : State->ResourceTables;
		for (UINT Index = 0; Index < Entry.Values.size(); ++Index)
		{
			WriteDescriptor(*State, Arena.Cpu(Set->Tables[Slot.Table].Offset + Slot.Offset + Index),
			                Entry.Values[Index], Layout.Description.Slots[Entry.Slot].Kind);
		}
	}
	++State->BindingSetsCreated;
	return {std::move(Set)};
}

void ValidateGraphicsBindings(const FDrawPacket& InDraw, const FD3D12Pipeline& InPipeline,
                              const FD3D12DeviceState& InState)
{
	const FD3D12BindingLayout& Layout = NativeResource<FD3D12BindingLayout>(InPipeline.Layout.Payload, &InState);
	if (InDraw.Bindings)
	{
		const FD3D12BindingSet& Set = NativeResource<FD3D12BindingSet>(InDraw.Bindings.Payload, &InState);
		if (Set.Description.Layout.Payload != InPipeline.Layout.Payload)
		{
			throw std::invalid_argument("Incompatible pipeline/resource binding layout");
		}
		for (const FResourceBindingEntry& Entry : Set.Description.Entries)
		{
			for (const FResourceBindingValue& Value : Entry.Values)
			{
				if (const auto* Texture = std::get_if<FTexture>(&Value))
				{
					const FD3D12Texture& Native = NativeResource<FD3D12Texture>(Texture->Payload, &InState);
					if (Native.UploadFence > InState.Fence->GetCompletedValue())
					{
						throw std::invalid_argument("Texture upload is not ready for graphics binding");
					}
				}
			}
		}
	}
	else if (!Layout.Tables.empty())
	{
		throw std::invalid_argument("Missing graphics resource binding set");
	}
	std::vector<bool> Seen(Layout.Slots.size());
	for (const FConstantBinding& Constant : InDraw.ConstantBindings)
	{
		if (Constant.Slot >= Seen.size() || Seen[Constant.Slot] ||
		    Layout.Description.Slots[Constant.Slot].Kind != ERHIBindingKind::ConstantBuffer)
		{
			throw std::invalid_argument("Invalid or duplicate dynamic constant binding");
		}
		Seen[Constant.Slot] = true;
		ValidateConstant(Constant, Layout.Description.Slots[Constant.Slot], InState);
	}
	for (std::size_t Index = 0; Index < Seen.size(); ++Index)
	{
		if (!Seen[Index] && Layout.Description.Slots[Index].Kind == ERHIBindingKind::ConstantBuffer)
		{
			throw std::invalid_argument("Missing dynamic constant binding");
		}
	}
}

void RecordGraphicsBindings(ID3D12GraphicsCommandList& InList, const FDrawPacket& InDraw,
                            const FD3D12Pipeline& InPipeline, const FD3D12DeviceState& InState,
                            FD3D12GraphicsBindingState& InBindings)
{
	HYP_PERF_SCOPE_C(Detail, RecordGraphicsBindings);
	const FD3D12BindingLayout& Layout = NativeResource<FD3D12BindingLayout>(InPipeline.Layout.Payload, &InState);
	if (InBindings.Root != InPipeline.Root.Get())
	{
		InList.SetGraphicsRootSignature(InPipeline.Root.Get());
		InBindings.Root = InPipeline.Root.Get();
		InBindings.Constants.fill(0);
		InBindings.Tables.fill(0);
		++InBindings.RootBinds;
	}
	const std::array<ID3D12DescriptorHeap*, 2> Heaps{InState.ResourceTables.GetHeap(), InState.SamplerTables.GetHeap()};
	if (InBindings.Heaps != Heaps)
	{
		InList.SetDescriptorHeaps(2, Heaps.data());
		InBindings.Heaps = Heaps;
		InBindings.Tables.fill(0);
		++InBindings.HeapBinds;
	}
	for (const FConstantBinding& Constant : InDraw.ConstantBindings)
	{
		const FD3D12Buffer& Buffer = NativeResource<FD3D12Buffer>(Constant.Slice.Buffer.Payload, &InState);
		const auto Root = Layout.Slots[Constant.Slot].RootParameter;
		const auto Address = Buffer.Resource->GetGPUVirtualAddress() + Constant.Slice.Offset;
		if (InBindings.Constants.at(Root) != Address)
		{
			InList.SetGraphicsRootConstantBufferView(Root, Address);
			InBindings.Constants[Root] = Address;
			++InBindings.ConstantBinds;
		}
	}
	if (InDraw.Bindings)
	{
		const FD3D12BindingSet& Set = NativeResource<FD3D12BindingSet>(InDraw.Bindings.Payload, &InState);
		for (std::size_t Index = 0; Index < Layout.Tables.size(); ++Index)
		{
			const auto& Arena = Layout.Tables[Index].bSampler ? InState.SamplerTables : InState.ResourceTables;
			const auto Root = Layout.Tables[Index].RootParameter;
			const auto Handle = Arena.Gpu(Set.Tables[Index].Offset);
			if (InBindings.Tables.at(Root) != Handle.ptr)
			{
				InList.SetGraphicsRootDescriptorTable(Root, Handle);
				InBindings.Tables[Root] = Handle.ptr;
				++InBindings.TableBinds;
			}
		}
	}
}
} // namespace Hyperion
