#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include <stdexcept>

namespace Hyperion
{
namespace
{
ERHIFill Convert(EMaterialFill InValue)
{
	switch (InValue)
	{
		case EMaterialFill::Solid:
			return ERHIFill::Solid;
		case EMaterialFill::Wireframe:
			return ERHIFill::Wireframe;
	}
	throw std::invalid_argument("Unsupported material Fill");
}

ERHICull Convert(EMaterialCull InValue)
{
	switch (InValue)
	{
		case EMaterialCull::None:
			return ERHICull::None;
		case EMaterialCull::Front:
			return ERHICull::Front;
		case EMaterialCull::Back:
			return ERHICull::Back;
	}
	throw std::invalid_argument("Unsupported material Cull");
}

ERHICompare Convert(EMaterialCompare InValue)
{
	switch (InValue)
	{
		case EMaterialCompare::Never:
			return ERHICompare::Never;
		case EMaterialCompare::Less:
			return ERHICompare::Less;
		case EMaterialCompare::Equal:
			return ERHICompare::Equal;
		case EMaterialCompare::LessEqual:
			return ERHICompare::LessEqual;
		case EMaterialCompare::Greater:
			return ERHICompare::Greater;
		case EMaterialCompare::NotEqual:
			return ERHICompare::NotEqual;
		case EMaterialCompare::GreaterEqual:
			return ERHICompare::GreaterEqual;
		case EMaterialCompare::Always:
			return ERHICompare::Always;
	}
	throw std::invalid_argument("Unsupported material Compare");
}

ERHIStencilOp Convert(EMaterialStencilOp InValue)
{
	switch (InValue)
	{
		case EMaterialStencilOp::Keep:
			return ERHIStencilOp::Keep;
		case EMaterialStencilOp::Zero:
			return ERHIStencilOp::Zero;
		case EMaterialStencilOp::Replace:
			return ERHIStencilOp::Replace;
		case EMaterialStencilOp::IncrementClamp:
			return ERHIStencilOp::IncrementClamp;
		case EMaterialStencilOp::DecrementClamp:
			return ERHIStencilOp::DecrementClamp;
		case EMaterialStencilOp::Invert:
			return ERHIStencilOp::Invert;
		case EMaterialStencilOp::IncrementWrap:
			return ERHIStencilOp::IncrementWrap;
		case EMaterialStencilOp::DecrementWrap:
			return ERHIStencilOp::DecrementWrap;
	}
	throw std::invalid_argument("Unsupported material StencilOp");
}

ERHIBlendFactor Convert(EMaterialBlendFactor InValue)
{
	switch (InValue)
	{
		case EMaterialBlendFactor::Zero:
			return ERHIBlendFactor::Zero;
		case EMaterialBlendFactor::One:
			return ERHIBlendFactor::One;
		case EMaterialBlendFactor::SourceColor:
			return ERHIBlendFactor::SourceColor;
		case EMaterialBlendFactor::InverseSourceColor:
			return ERHIBlendFactor::InverseSourceColor;
		case EMaterialBlendFactor::SourceAlpha:
			return ERHIBlendFactor::SourceAlpha;
		case EMaterialBlendFactor::InverseSourceAlpha:
			return ERHIBlendFactor::InverseSourceAlpha;
		case EMaterialBlendFactor::DestinationAlpha:
			return ERHIBlendFactor::DestinationAlpha;
		case EMaterialBlendFactor::InverseDestinationAlpha:
			return ERHIBlendFactor::InverseDestinationAlpha;
		case EMaterialBlendFactor::DestinationColor:
			return ERHIBlendFactor::DestinationColor;
		case EMaterialBlendFactor::InverseDestinationColor:
			return ERHIBlendFactor::InverseDestinationColor;
		case EMaterialBlendFactor::SourceAlphaSaturate:
			return ERHIBlendFactor::SourceAlphaSaturate;
		case EMaterialBlendFactor::Constant:
			return ERHIBlendFactor::Constant;
		case EMaterialBlendFactor::InverseConstant:
			return ERHIBlendFactor::InverseConstant;
	}
	throw std::invalid_argument("Unsupported material BlendFactor");
}

ERHIBlendOp Convert(EMaterialBlendOp InValue)
{
	switch (InValue)
	{
		case EMaterialBlendOp::Add:
			return ERHIBlendOp::Add;
		case EMaterialBlendOp::Subtract:
			return ERHIBlendOp::Subtract;
		case EMaterialBlendOp::ReverseSubtract:
			return ERHIBlendOp::ReverseSubtract;
		case EMaterialBlendOp::Minimum:
			return ERHIBlendOp::Minimum;
		case EMaterialBlendOp::Maximum:
			return ERHIBlendOp::Maximum;
	}
	throw std::invalid_argument("Unsupported material BlendOp");
}

FStencilFaceDesc Convert(const FMaterialStencilFace& InFace)
{
	return {Convert(InFace.Compare), Convert(InFace.Fail), Convert(InFace.DepthFail), Convert(InFace.Pass)};
}
} // namespace

FGraphicsState ConvertMaterialState(const FMaterialState& InState, bool bInMirrored)
{
	const auto State = NormalizeMaterialState(InState);
	FGraphicsState Result;
	Result.Fill = Convert(State.Fill);
	Result.Cull = Convert(State.Cull);
	Result.DepthCompare = Convert(State.DepthCompare);
	Result.FrontStencil = Convert(State.FrontStencil);
	Result.BackStencil = Convert(State.BackStencil);
	Result.SourceRgb = Convert(State.SourceRgb);
	Result.DestinationRgb = Convert(State.DestinationRgb);
	Result.RgbOperation = Convert(State.RgbOperation);
	Result.SourceAlpha = Convert(State.SourceAlpha);
	Result.DestinationAlpha = Convert(State.DestinationAlpha);
	Result.AlphaOperation = Convert(State.AlphaOperation);
	Result.DepthBias = State.DepthBias;
	Result.DepthBiasClamp = State.DepthBiasClamp;
	Result.SlopeScaledDepthBias = State.SlopeScaledDepthBias;
	Result.bDepthClip = State.bDepthClip;
	Result.bDepthTest = State.bDepthTest;
	Result.bDepthWrite = State.bDepthWrite;
	Result.bStencil = State.bStencil;
	Result.StencilReadMask = State.StencilReadMask;
	Result.StencilWriteMask = State.StencilWriteMask;
	Result.bBlend = State.bBlend;
	Result.ColorWriteMask = State.ColorWriteMask;
	Result.bAlphaToCoverage = State.bAlphaToCoverage;
	Result.SampleMask = State.SampleMask;
	Result.bFrontCounterClockwise = State.bFrontCounterClockwise != bInMirrored;
	return Result;
}

FGraphicsDynamicState ConvertMaterialDynamicState(const FMaterialDynamicState& InState)
{
	FGraphicsDynamicState Result{InState.StencilReference, InState.BlendConstants};
	ValidateGraphicsDynamicState(Result);
	return Result;
}

FResourceBindingLayoutDesc DescribeMaterialLayout(const FCompiledMaterialPass& InPass)
{
	FResourceBindingLayoutDesc Result;
	for (const auto& Binding : InPass.Bindings)
	{
		FResourceBindingSlot Slot;
		switch (Binding.Resource.Kind)
		{
			case EBindingKind::UniformBuffer:
				Slot.Kind = ERHIBindingKind::ConstantBuffer;
				break;
			case EBindingKind::Texture:
				Slot.Kind = ERHIBindingKind::Texture2D;
				break;
			case EBindingKind::Sampler:
				Slot.Kind = ERHIBindingKind::Sampler;
				break;
			case EBindingKind::StructuredBuffer:
				Slot.Kind = ERHIBindingKind::StructuredBuffer;
				break;
			case EBindingKind::RawBuffer:
				Slot.Kind = ERHIBindingKind::RawBuffer;
				break;
			case EBindingKind::Unsupported:
				throw std::invalid_argument("Unsupported material binding");
		}
		switch (Binding.Stages)
		{
			case 1:
				Slot.Visibility = ERHIShaderVisibility::Vertex;
				break;
			case 2:
				Slot.Visibility = ERHIShaderVisibility::Pixel;
				break;
			case 3:
				Slot.Visibility = ERHIShaderVisibility::Graphics;
				break;
			default:
				throw std::invalid_argument("Unsupported material shader stage");
		}
		Slot.Register = Binding.Resource.Register;
		Slot.Space = Binding.Resource.Space;
		Slot.Count = Binding.Resource.Count;
		Slot.MinimumBufferSize = Binding.Resource.ByteSize;
		Slot.StructureByteStride = Binding.Resource.StructureByteStride;
		Slot.InstanceStride = Binding.InstanceStride;
		Slot.InstanceCapacity = Binding.InstanceCapacity;
		Slot.bComparison = Binding.Resource.bComparison;
		Result.Slots.push_back(Slot);
	}
	return Result;
}

FPipelineDesc DescribeMaterialPipeline(const FCompiledMaterialPass& InProgram, const FMaterialPass& InPass,
                                       const FResourceBindingLayout& InLayout,
                                       std::vector<FVertexAttribute> InAttributes, std::uint32_t InStride,
                                       ERHIPrimitiveTopology InTopology, FGraphicsTarget InTarget, bool bInMirrored)
{
	FMaterialPassContext Context;
	Context.ColorTargetCount = InTarget.ColorCount;
	Context.SampleCount = InTarget.SampleCount;
	Context.bHasDepth = InTarget.Depth != ERHIDepthFormat::None;
	Context.bHasStencil = InTarget.Depth == ERHIDepthFormat::D32S8;
	const auto Availability = ResolveMaterialPass(InPass, Context);
	if (!Availability.IsAvailable() || InTarget.bSrgb != InPass.bSrgbTarget)
	{
		throw std::invalid_argument("Incompatible material target: " + Availability.Reason);
	}
	FPipelineDesc Result;
	Result.Vertex = InProgram.Vertex;
	Result.Pixel = InProgram.Pixel;
	Result.Attributes = std::move(InAttributes);
	Result.VertexStride = InStride;
	Result.Topology = InTopology;
	Result.Layout = InLayout;
	Result.State = ConvertMaterialState(Availability.State, bInMirrored);
	Result.Target = InTarget;
	ValidateGraphicsPipeline(Result);
	return Result;
}
} // namespace Hyperion
