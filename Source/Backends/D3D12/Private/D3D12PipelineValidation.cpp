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
			if (InBinding.Dimension == EShaderResourceDimension::Texture2D &&
			    InBinding.ResourceScalar == EShaderScalar::Float)
			{
				return ERHIBindingKind::Texture2D;
			}
			break;
		case EBindingKind::Sampler:
			if (!InBinding.bComparison)
			{
				return ERHIBindingKind::Sampler;
			}
			break;
		case EBindingKind::StructuredBuffer:
			return ERHIBindingKind::StructuredBuffer;
		case EBindingKind::RawBuffer:
			return ERHIBindingKind::RawBuffer;
		case EBindingKind::Unsupported:
			break;
	}
	throw std::invalid_argument("Unsupported graphics shader resource: " + InBinding.Name);
}

void ValidateStage(const FShaderArtifact& InShader, const FResourceBindingLayoutDesc& InLayout)
{
	const unsigned Stage = InShader.Stage == EShaderStage::Vertex ? 1U : 2U;
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
		    (Kind == ERHIBindingKind::ConstantBuffer && Slot->MinimumBufferSize < Binding.ByteSize) ||
		    (Kind == ERHIBindingKind::StructuredBuffer &&
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
} // namespace Hyperion
