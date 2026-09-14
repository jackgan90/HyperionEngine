#include "D3D12GraphicsState.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
ERHIBindingKind BindingKind(const FShaderBinding& InBinding)
{
	switch (InBinding.Kind)
	{
		case EBindingKind::UniformBuffer:
			return ERHIBindingKind::ConstantBuffer;
		case EBindingKind::Texture:
			if ((InBinding.Dimension == EShaderResourceDimension::Texture2D ||
			     InBinding.Dimension == EShaderResourceDimension::TextureCube) &&
			    InBinding.ResourceScalar == EShaderScalar::Float)
			{
				return InBinding.Dimension == EShaderResourceDimension::TextureCube ? ERHIBindingKind::TextureCube
				                                                                    : ERHIBindingKind::Texture2D;
			}
			break;
		case EBindingKind::Sampler:
			return ERHIBindingKind::Sampler;
		case EBindingKind::StructuredBuffer:
			return ERHIBindingKind::StructuredBuffer;
		case EBindingKind::RawBuffer:
			return ERHIBindingKind::RawBuffer;
		case EBindingKind::StorageStructuredBuffer:
			return ERHIBindingKind::StorageStructuredBuffer;
		case EBindingKind::StorageRawBuffer:
			return ERHIBindingKind::StorageRawBuffer;
		case EBindingKind::StorageTexture:
			if (InBinding.Dimension == EShaderResourceDimension::Texture2D &&
			    InBinding.ResourceScalar == EShaderScalar::Float)
			{
				return ERHIBindingKind::StorageTexture2D;
			}
			break;
		case EBindingKind::Unsupported:
			break;
	}
	throw std::invalid_argument("Unsupported graphics shader resource: " + InBinding.Name);
}

void ValidateStage(const FShaderArtifact& InShader, const FResourceBindingLayoutDesc& InLayout)
{
	const unsigned Stage =
	    InShader.Stage == EShaderStage::Vertex ? 1U : (InShader.Stage == EShaderStage::Pixel ? 2U : 4U);
	for (const auto& Binding : InShader.Bindings)
	{
		const auto Kind = BindingKind(Binding);
		const auto Slot =
		    std::find_if(InLayout.Slots.begin(), InLayout.Slots.end(),
		                 [&](const FResourceBindingSlot& InSlot)
		                 {
			                 return InSlot.Kind == Kind && (static_cast<unsigned>(InSlot.Visibility) & Stage) != 0 &&
			                        InSlot.Space == Binding.Space && InSlot.Register <= Binding.Register &&
			                        static_cast<std::uint64_t>(InSlot.Register) + InSlot.Count >=
			                            static_cast<std::uint64_t>(Binding.Register) + Binding.Count;
		                 });
		if (Slot == InLayout.Slots.end() ||
		    (Slot->Kind == ERHIBindingKind::Sampler && Slot->bComparison != Binding.bComparison) ||
		    (Kind == ERHIBindingKind::ConstantBuffer && Slot->MinimumBufferSize < Binding.ByteSize) ||
		    ((Kind == ERHIBindingKind::StructuredBuffer || Kind == ERHIBindingKind::StorageStructuredBuffer) &&
		     (Binding.StructureByteStride == 0 || Slot->StructureByteStride != Binding.StructureByteStride)))
		{
			throw std::invalid_argument("Graphics binding layout does not cover shader resource: " + Binding.Name);
		}
		if (Slot->InstanceStride)
		{
			if (Binding.Members.size() != 1 || Binding.Members.front().Kind != EShaderValueKind::Array ||
			    Binding.Members.front().Offset != 0 || Binding.Members.front().ArrayStride != Slot->InstanceStride ||
			    Binding.Members.front().ArrayCount != Slot->InstanceCapacity ||
			    Binding.Members.front().Members.size() != 1 ||
			    Binding.Members.front().Members.front().Kind != EShaderValueKind::Structure)
			{
				throw std::invalid_argument("Graphics instance layout does not match reflected record array: " +
				                            Binding.Name);
			}
		}
	}
}
} // namespace

void ValidatePipelineBindings(const FPipelineDesc& InDesc, const FResourceBindingLayoutDesc& InLayout)
{
	ValidateStage(InDesc.Vertex, InLayout);
	ValidateStage(InDesc.Pixel, InLayout);
}

void ValidatePipelineBindings(const FComputePipelineDesc& InDesc, const FResourceBindingLayoutDesc& InLayout)
{
	for (const auto& Slot : InLayout.Slots)
	{
		if (Slot.Visibility != ERHIShaderVisibility::Compute)
		{
			throw std::invalid_argument("Compute pipeline requires compute binding layout");
		}
	}
	ValidateStage(InDesc.Compute, InLayout);
}
} // namespace Hyperion
