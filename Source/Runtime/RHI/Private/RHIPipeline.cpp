#include "Hyperion/RHI/RHIPipeline.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <stdexcept>

namespace Hyperion
{
FVertexFormatInfo GetVertexFormatInfo(EVertexFormat InFormat)
{
	switch (InFormat)
	{
		case EVertexFormat::Float:
			return {EShaderScalar::Float, 1, 4};
		case EVertexFormat::Float2:
			return {EShaderScalar::Float, 2, 8};
		case EVertexFormat::Float3:
			return {EShaderScalar::Float, 3, 12};
		case EVertexFormat::Float4:
			return {EShaderScalar::Float, 4, 16};
		case EVertexFormat::Unorm8x4:
			return {EShaderScalar::Float, 4, 4};
		case EVertexFormat::Int:
			return {EShaderScalar::Int, 1, 4};
		case EVertexFormat::Int2:
			return {EShaderScalar::Int, 2, 8};
		case EVertexFormat::Int3:
			return {EShaderScalar::Int, 3, 12};
		case EVertexFormat::Int4:
			return {EShaderScalar::Int, 4, 16};
		case EVertexFormat::Uint:
			return {EShaderScalar::Uint, 1, 4};
		case EVertexFormat::Uint2:
			return {EShaderScalar::Uint, 2, 8};
		case EVertexFormat::Uint3:
			return {EShaderScalar::Uint, 3, 12};
		case EVertexFormat::Uint4:
			return {EShaderScalar::Uint, 4, 16};
	}
	throw std::invalid_argument("Unsupported vertex format");
}

namespace
{
std::string CanonicalSemantic(std::string InName)
{
	std::transform(InName.begin(), InName.end(), InName.begin(),
	               [](unsigned char InCharacter)
	               {
		               return static_cast<char>(std::toupper(InCharacter));
	               });
	return InName;
}

void ValidateVertexInput(const FPipelineDesc& InDesc)
{
	std::set<std::pair<std::string, std::uint32_t>> Names;
	for (const auto& Attribute : InDesc.Attributes)
	{
		const auto Format = GetVertexFormatInfo(Attribute.Format);
		if (Attribute.Semantic.empty() ||
		    !Names.emplace(CanonicalSemantic(Attribute.Semantic), Attribute.SemanticIndex).second ||
		    Attribute.Offset > InDesc.VertexStride || Format.Bytes > InDesc.VertexStride - Attribute.Offset)
		{
			throw std::invalid_argument("Invalid vertex attribute semantic or stride range");
		}
	}
	for (const auto& Input : InDesc.Vertex.Reflection.Inputs)
	{
		if (Input.bSystemValue)
		{
			continue;
		}
		const auto Attribute =
		    std::find_if(InDesc.Attributes.begin(), InDesc.Attributes.end(),
		                 [&](const FVertexAttribute& InAttribute)
		                 {
			                 return CanonicalSemantic(InAttribute.Semantic) == CanonicalSemantic(Input.Semantic) &&
			                        InAttribute.SemanticIndex == Input.SemanticIndex;
		                 });
		if (Attribute == InDesc.Attributes.end())
		{
			throw std::invalid_argument("Missing vertex shader input: " + Input.Semantic);
		}
		const auto Format = GetVertexFormatInfo(Attribute->Format);
		if (Format.Scalar != Input.Scalar || Format.Components < Input.Components)
		{
			throw std::invalid_argument("Vertex shader input type mismatch: " + Input.Semantic);
		}
	}
}

void ValidateShaderOutputs(const FPipelineDesc& InDesc)
{
	if (InDesc.Vertex.Stage != EShaderStage::Vertex || InDesc.Vertex.Bytes.empty() ||
	    (!InDesc.Pixel.Bytes.empty() && InDesc.Pixel.Stage != EShaderStage::Pixel))
	{
		throw std::invalid_argument("Graphics pipeline shader stage mismatch");
	}
	if (InDesc.Pixel.Bytes.empty() && InDesc.State.ColorWriteMask != 0)
	{
		throw std::invalid_argument("A pipeline without pixel shader must disable color writes");
	}
	for (const auto& Input : InDesc.Pixel.Reflection.Inputs)
	{
		const auto Semantic = CanonicalSemantic(Input.Semantic);
		if (Semantic == "SV_ISFRONTFACE" || Semantic == "SV_PRIMITIVEID")
		{
			continue;
		}
		const auto Output =
		    std::find_if(InDesc.Vertex.Reflection.Outputs.begin(), InDesc.Vertex.Reflection.Outputs.end(),
		                 [&](const FShaderSignatureParameter& InOutput)
		                 {
			                 return CanonicalSemantic(InOutput.Semantic) == Semantic &&
			                        InOutput.SemanticIndex == Input.SemanticIndex && InOutput.Scalar == Input.Scalar &&
			                        InOutput.Components >= Input.Components;
		                 });
		if (Output == InDesc.Vertex.Reflection.Outputs.end())
		{
			throw std::invalid_argument("Vertex/pixel shader interface mismatch: " + Input.Semantic);
		}
	}
	for (const auto& Output : InDesc.Pixel.Reflection.Outputs)
	{
		const auto Semantic = CanonicalSemantic(Output.Semantic);
		if ((Semantic == "SV_TARGET" &&
		     (Output.SemanticIndex >= InDesc.Target.ColorCount || Output.Scalar != EShaderScalar::Float)) ||
		    (Semantic != "SV_TARGET" && Semantic != "SV_DEPTH" && Semantic != "SV_DEPTHLESSEQUAL" &&
		     Semantic != "SV_DEPTHGREATEREQUAL"))
		{
			throw std::invalid_argument("Unsupported pixel shader output: " + Output.Semantic);
		}
		if (Semantic.starts_with("SV_DEPTH") && InDesc.Target.Depth == ERHIDepthFormat::None)
		{
			throw std::invalid_argument("Pixel depth output requires a depth target");
		}
	}
}

bool IsAlphaFactor(ERHIBlendFactor InFactor)
{
	return InFactor != ERHIBlendFactor::SourceColor && InFactor != ERHIBlendFactor::InverseSourceColor &&
	       InFactor != ERHIBlendFactor::DestinationColor && InFactor != ERHIBlendFactor::InverseDestinationColor &&
	       InFactor != ERHIBlendFactor::SourceAlphaSaturate;
}

void ValidateStencil(const FStencilFaceDesc& InFace)
{
	if (InFace.Compare > ERHICompare::Always || InFace.Fail > ERHIStencilOp::DecrementWrap ||
	    InFace.DepthFail > ERHIStencilOp::DecrementWrap || InFace.Pass > ERHIStencilOp::DecrementWrap)
	{
		throw std::invalid_argument("Invalid stencil face state");
	}
}
} // namespace

void ValidateGraphicsPipeline(const FPipelineDesc& InDesc)
{
	const auto& State = InDesc.State;
	if (InDesc.Target.SampleCount != 1 || InDesc.Target.ColorCount > MaximumColorTargets || State.bAlphaToCoverage ||
	    (InDesc.Target.ColorCount == 0 && (State.ColorWriteMask != 0 || State.bBlend || InDesc.Target.bSrgb ||
	                                       InDesc.Target.Depth == ERHIDepthFormat::None)) ||
	    InDesc.Target.Depth > ERHIDepthFormat::D32S8 || InDesc.Topology > ERHIPrimitiveTopology::PointList)
	{
		throw std::invalid_argument(
		    "Invalid graphics attachment count/state; single-sample zero through eight color targets are supported");
	}
	if ((State.bDepthTest && InDesc.Target.Depth == ERHIDepthFormat::None) ||
	    (State.bStencil && InDesc.Target.Depth != ERHIDepthFormat::D32S8) || (State.bDepthWrite && !State.bDepthTest))
	{
		throw std::invalid_argument("Incompatible depth/stencil state and target");
	}
	if (State.Fill > ERHIFill::Wireframe || State.Cull > ERHICull::Back || State.DepthCompare > ERHICompare::Always ||
	    State.SourceRgb > ERHIBlendFactor::InverseConstant || State.DestinationRgb > ERHIBlendFactor::InverseConstant ||
	    State.SourceAlpha > ERHIBlendFactor::InverseConstant ||
	    State.DestinationAlpha > ERHIBlendFactor::InverseConstant || State.RgbOperation > ERHIBlendOp::Maximum ||
	    State.AlphaOperation > ERHIBlendOp::Maximum || State.ColorWriteMask > 15 ||
	    !std::isfinite(State.DepthBiasClamp) || !std::isfinite(State.SlopeScaledDepthBias) ||
	    (State.bBlend && (!IsAlphaFactor(State.SourceAlpha) || !IsAlphaFactor(State.DestinationAlpha))))
	{
		throw std::invalid_argument("Invalid graphics pipeline state or alpha blend factor");
	}
	for (std::uint32_t Index = 0; Index < InDesc.Target.ColorCount; ++Index)
	{
		if (InDesc.Target.ColorFormats[Index] >= ERHIColorFormat::Count ||
		    (Index == 0 && InDesc.Target.bSrgb && InDesc.Target.ColorFormats[Index] != ERHIColorFormat::Rgba8Unorm))
		{
			throw std::invalid_argument("Invalid color format or incompatible sRGB view");
		}
	}
	ValidateStencil(State.FrontStencil);
	ValidateStencil(State.BackStencil);
	ValidateVertexInput(InDesc);
	ValidateShaderOutputs(InDesc);
}

void ValidateGraphicsDynamicState(const FGraphicsDynamicState& InState)
{
	if (InState.StencilReference > 255 || !std::all_of(InState.BlendConstants.begin(), InState.BlendConstants.end(),
	                                                   [](float InValue)
	                                                   {
		                                                   return std::isfinite(InValue);
	                                                   }))
	{
		throw std::invalid_argument("Invalid dynamic stencil reference or blend constants");
	}
}

void ValidateViewport(const FViewport& InViewport, FSize InTargetSize)
{
	const std::array Values{InViewport.X,      InViewport.Y,        InViewport.Width,
	                        InViewport.Height, InViewport.MinDepth, InViewport.MaxDepth};
	if (!std::all_of(Values.begin(), Values.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }) ||
	    InViewport.X < 0 || InViewport.Y < 0 || InViewport.Width <= 0 || InViewport.Height <= 0 ||
	    InViewport.X + InViewport.Width > InTargetSize.Width ||
	    InViewport.Y + InViewport.Height > InTargetSize.Height || InViewport.MinDepth < 0 || InViewport.MaxDepth > 1 ||
	    InViewport.MinDepth > InViewport.MaxDepth)
	{
		throw std::invalid_argument("Viewport is outside the target or depth range");
	}
}
} // namespace Hyperion
