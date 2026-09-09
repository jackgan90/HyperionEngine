#include "D3D12RHIDevice.h"
#include "D3D12Resources.h"
#include <array>

namespace Hyperion
{
namespace
{
D3D12_SHADER_VISIBILITY NativeVisibility(ERHIShaderVisibility InVisibility)
{
	switch (InVisibility)
	{
		case ERHIShaderVisibility::Vertex:
			return D3D12_SHADER_VISIBILITY_VERTEX;
		case ERHIShaderVisibility::Pixel:
			return D3D12_SHADER_VISIBILITY_PIXEL;
		case ERHIShaderVisibility::Graphics:
			return D3D12_SHADER_VISIBILITY_ALL;
		default:
			throw std::invalid_argument("Invalid graphics binding visibility");
	}
}

std::uint32_t RegisterClass(ERHIBindingKind InKind)
{
	if (InKind == ERHIBindingKind::ConstantBuffer)
	{
		return 0;
	}
	if (InKind == ERHIBindingKind::Sampler)
	{
		return 2;
	}
	return 1;
}

void ValidateLayout(const FResourceBindingLayoutDesc& InDesc, const FRHICapabilities& InCaps)
{
	std::array<std::array<std::uint32_t, 3>, 2> Counts{};
	for (std::size_t Index = 0; Index < InDesc.Slots.size(); ++Index)
	{
		const FResourceBindingSlot& Slot = InDesc.Slots[Index];
		NativeVisibility(Slot.Visibility);
		if (Slot.bComparison && (Slot.Kind != ERHIBindingKind::Sampler || !InCaps.bComparisonSamplers))
		{
			throw std::invalid_argument("Invalid or unsupported comparison sampler slot");
		}
		if ((Slot.InstanceStride == 0) != (Slot.InstanceCapacity == 0) ||
		    (Slot.InstanceStride &&
		     (Slot.Kind != ERHIBindingKind::ConstantBuffer || Slot.InstanceStride % 16 ||
		      std::uint64_t(Slot.InstanceStride) * Slot.InstanceCapacity != Slot.MinimumBufferSize)))
		{
			throw std::invalid_argument("Invalid instance constant layout");
		}
		if (Slot.Kind < ERHIBindingKind::ConstantBuffer || Slot.Kind > ERHIBindingKind::Sampler || Slot.Count == 0 ||
		    Slot.Space >= InCaps.MaxRegisterSpaces || Slot.Register >= ShaderRegistersPerKind ||
		    Slot.Count > ShaderRegistersPerKind - Slot.Register || Slot.MinimumBufferSize > InCaps.MaxConstantRange ||
		    (Slot.Kind == ERHIBindingKind::ConstantBuffer && Slot.Count != 1) ||
		    (Slot.StructureByteStride != 0 && (Slot.Kind != ERHIBindingKind::StructuredBuffer ||
		                                       Slot.StructureByteStride > 2048 || Slot.StructureByteStride % 4 != 0)))
		{
			throw std::invalid_argument("Invalid binding kind, count, space, register or constant range");
		}
		const std::uint32_t Class = RegisterClass(Slot.Kind);
		for (std::size_t Stage = 0; Stage < Counts.size(); ++Stage)
		{
			if ((static_cast<unsigned>(Slot.Visibility) & (1U << Stage)) != 0)
			{
				Counts[Stage][Class] += Slot.Count;
			}
		}
		for (std::size_t Previous = 0; Previous < Index; ++Previous)
		{
			const FResourceBindingSlot& Other = InDesc.Slots[Previous];
			if (Slot.Space == Other.Space && Class == RegisterClass(Other.Kind) &&
			    (static_cast<unsigned>(Slot.Visibility) & static_cast<unsigned>(Other.Visibility)) != 0 &&
			    Slot.Register < Other.Register + Other.Count && Other.Register < Slot.Register + Slot.Count)
			{
				throw std::invalid_argument("Overlapping RHI binding registers and stages");
			}
		}
	}
	for (const auto& Stage : Counts)
	{
		if (Stage[0] > InCaps.MaxConstantBuffers || Stage[1] > InCaps.MaxSampledTextures ||
		    Stage[2] > InCaps.MaxSamplers)
		{
			throw std::invalid_argument("Graphics binding layout exceeds per-stage resource limits");
		}
	}
}

UINT PlanLayout(FD3D12BindingLayout& InLayout)
{
	UINT RootCount{};
	UINT Cost{};
	for (const FResourceBindingSlot& Slot : InLayout.Description.Slots)
	{
		FD3D12BindingLayout::FSlot Plan;
		if (Slot.Kind == ERHIBindingKind::ConstantBuffer)
		{
			Plan.RootParameter = RootCount++;
			Cost += 2;
		}
		else
		{
			const bool bSampler = Slot.Kind == ERHIBindingKind::Sampler;
			UINT Table{};
			for (; Table < InLayout.Tables.size(); ++Table)
			{
				if (InLayout.Tables[Table].bSampler == bSampler && InLayout.Tables[Table].Visibility == Slot.Visibility)
				{
					break;
				}
			}
			if (Table == InLayout.Tables.size())
			{
				InLayout.Tables.push_back({RootCount++, bSampler, Slot.Visibility});
				++Cost;
			}
			auto& Group = InLayout.Tables[Table];
			Plan.RootParameter = Group.RootParameter;
			Plan.Table = Table;
			Plan.Offset = Group.Count;
			Group.Ranges.push_back({bSampler ? D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER : D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			                        Slot.Count, Slot.Register, Slot.Space, Group.Count});
			Group.Count += Slot.Count;
		}
		InLayout.Slots.push_back(Plan);
	}
	if (Cost > 64)
	{
		throw std::invalid_argument("Graphics binding layout exceeds root-signature budget");
	}
	return RootCount;
}
} // namespace

FResourceBindingLayout FD3D12RHIDevice::CreateBindingLayout(const FResourceBindingLayoutDesc& InDesc)
{
	ValidateLayout(InDesc, State->Capabilities);
	auto Layout = std::make_shared<FD3D12BindingLayout>();
	Layout->State = State;
	Layout->Description = InDesc;
	std::vector<D3D12_ROOT_PARAMETER> Parameters(PlanLayout(*Layout));
	for (std::size_t Index = 0; Index < InDesc.Slots.size(); ++Index)
	{
		const FResourceBindingSlot& Slot = InDesc.Slots[Index];
		if (Slot.Kind == ERHIBindingKind::ConstantBuffer)
		{
			auto& Parameter = Parameters[Layout->Slots[Index].RootParameter];
			Layout->ConstantMask |= std::uint64_t{1} << Layout->Slots[Index].RootParameter;
			Parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			Parameter.ShaderVisibility = NativeVisibility(Slot.Visibility);
			Parameter.Descriptor = {Slot.Register, Slot.Space};
		}
	}
	for (const FD3D12BindingLayout::FTable& Table : Layout->Tables)
	{
		auto& Parameter = Parameters[Table.RootParameter];
		Parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		Parameter.ShaderVisibility = NativeVisibility(Table.Visibility);
		Parameter.DescriptorTable = {static_cast<UINT>(Table.Ranges.size()), Table.Ranges.data()};
	}
	D3D12_ROOT_SIGNATURE_DESC Root{};
	Root.NumParameters = static_cast<UINT>(Parameters.size());
	Root.pParameters = Parameters.data();
	Root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> Blob;
	ComPtr<ID3DBlob> Error;
	const HRESULT Result = D3D12SerializeRootSignature(&Root, D3D_ROOT_SIGNATURE_VERSION_1, &Blob, &Error);
	if (FAILED(Result))
	{
		throw std::invalid_argument(
		    Error ? std::string(static_cast<const char*>(Error->GetBufferPointer()), Error->GetBufferSize())
		          : "Serialize material root signature failed");
	}
	Check(State->Device->CreateRootSignature(0, Blob->GetBufferPointer(), Blob->GetBufferSize(),
	                                         IID_PPV_ARGS(&Layout->Root)),
	      "Create material root signature");
	return {std::move(Layout)};
}
} // namespace Hyperion
