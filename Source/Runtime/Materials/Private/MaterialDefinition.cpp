#include "Hyperion/Materials/Material.h"
#include "MaterialIdentity.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace Hyperion
{
namespace
{
void ValidateStencil(const FMaterialStencilFace& InFace)
{
	if (InFace.Compare > EMaterialCompare::Always || InFace.Fail > EMaterialStencilOp::DecrementWrap ||
	    InFace.DepthFail > EMaterialStencilOp::DecrementWrap || InFace.Pass > EMaterialStencilOp::DecrementWrap)
	{
		throw std::invalid_argument("Invalid material stencil state");
	}
}

void NormalizeShader(FMaterialShader& InShader, bool bInRequired)
{
	if (InShader.Path.empty() != InShader.Entry.empty() || (bInRequired && InShader.Path.empty()))
	{
		throw std::invalid_argument("Material shader requires a source and entry point");
	}
	std::sort(InShader.Defines.begin(), InShader.Defines.end(),
	          [](const FMaterialShaderDefine& InA, const FMaterialShaderDefine& InB)
	          {
		          return InA.Name < InB.Name;
	          });
	std::string Previous;
	for (const FMaterialShaderDefine& Define : InShader.Defines)
	{
		if (Define.Name.empty() || Define.Name == Previous ||
		    Define.Name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
		        std::string::npos ||
		    (Define.Name.front() >= '0' && Define.Name.front() <= '9') ||
		    Define.Value.find_first_of("\r\n") != std::string::npos)
		{
			throw std::invalid_argument("Invalid or duplicate material shader define: " + Define.Name);
		}
		Previous = Define.Name;
	}
}

void ValidatePass(FMaterialPass& InPass)
{
	NormalizeShader(InPass.Vertex, true);
	NormalizeShader(InPass.Pixel, false);
	InPass.State = NormalizeMaterialState(InPass.State);
	std::set<std::string> InstanceBlocks;
	for (const auto& Array : InPass.InstanceArrays)
	{
		if (Array.Block.empty() || Array.Member.empty() || !InstanceBlocks.insert(Array.Block).second)
		{
			throw std::invalid_argument("Invalid or duplicate material instance array");
		}
	}
	if (InPass.Usage.empty() || InPass.Queue > EMaterialQueue::Overlay ||
	    (InPass.Pixel.Path.empty() && (InPass.State.ColorWriteMask != 0 || InPass.bAlphaClip)))
	{
		throw std::invalid_argument("Invalid material usage, queue or pixel-less color/alpha contract");
	}
	if (InPass.DynamicState.StencilReference > 255 ||
	    !std::all_of(InPass.DynamicState.BlendConstants.begin(), InPass.DynamicState.BlendConstants.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }))
	{
		throw std::invalid_argument("Invalid material dynamic state");
	}
}
} // namespace

FMaterialState NormalizeMaterialState(FMaterialState InState)
{
	ValidateStencil(InState.FrontStencil);
	ValidateStencil(InState.BackStencil);
	if (InState.Fill > EMaterialFill::Wireframe || InState.Cull > EMaterialCull::Back ||
	    InState.DepthCompare > EMaterialCompare::Always || InState.SourceRgb > EMaterialBlendFactor::InverseConstant ||
	    InState.DestinationRgb > EMaterialBlendFactor::InverseConstant ||
	    InState.SourceAlpha > EMaterialBlendFactor::InverseConstant ||
	    InState.DestinationAlpha > EMaterialBlendFactor::InverseConstant ||
	    InState.RgbOperation > EMaterialBlendOp::Maximum || InState.AlphaOperation > EMaterialBlendOp::Maximum ||
	    InState.ColorWriteMask > 15 || !std::isfinite(InState.DepthBiasClamp) ||
	    !std::isfinite(InState.SlopeScaledDepthBias))
	{
		throw std::invalid_argument("Invalid material graphics state");
	}
	if (InState.bDepthWrite && !InState.bDepthTest)
	{
		throw std::invalid_argument(
		    "Depth writes require depth testing; use Always comparison for unconditional writes");
	}
	if (!InState.bDepthTest)
	{
		InState.DepthCompare = EMaterialCompare::Always;
	}
	if (!InState.bStencil)
	{
		InState.FrontStencil = {};
		InState.BackStencil = {};
		InState.StencilReadMask = 255;
		InState.StencilWriteMask = 255;
	}
	if (!InState.bBlend)
	{
		InState.SourceRgb = InState.SourceAlpha = EMaterialBlendFactor::One;
		InState.DestinationRgb = InState.DestinationAlpha = EMaterialBlendFactor::Zero;
		InState.RgbOperation = InState.AlphaOperation = EMaterialBlendOp::Add;
	}
	return InState;
}

FMaterialPassResult ResolveMaterialPass(const FMaterialPass& InPass, const FMaterialPassContext& InContext)
{
	FMaterialPassResult Result;
	Result.State = NormalizeMaterialState(InPass.State);
	if (!InContext.bUsageScheduled)
	{
		Result.Availability = EMaterialAvailability::Unsupported;
		Result.Reason = "Renderer does not schedule usage " + InPass.Usage;
	}
	else if (InContext.SampleCount != 1 || InContext.ColorTargetCount > 1)
	{
		Result.Availability = EMaterialAvailability::Unsupported;
		Result.Reason = "Material renderer supports one single-sample color target";
	}
	else if (InPass.State.bAlphaToCoverage || (InPass.State.bDepthTest && !InContext.bHasDepth) ||
	         (InPass.State.bStencil && !InContext.bHasStencil) ||
	         (InPass.State.ColorWriteMask != 0 && InContext.ColorTargetCount == 0))
	{
		Result.Availability = EMaterialAvailability::Incompatible;
		Result.Reason = InPass.State.bAlphaToCoverage
		                    ? "Alpha-to-coverage requires multisampling, which is not enabled"
		                    : "Material pass requires an unavailable color/depth/stencil attachment";
	}
	return Result;
}

FMaterialDefinition::FMaterialDefinition(FMaterialDescription InDescription,
                                         std::shared_ptr<const FMaterialSemanticRegistry> InSemantics)
    : Identity(MaterialsPrivate::NextIdentity()), Description(std::move(InDescription))
{
	if (!InSemantics || Description.Name.empty() || Description.Version == 0 || Description.Passes.empty())
	{
		throw std::invalid_argument("Material requires a name, version, semantic registry and at least one pass");
	}
	auto Registry = std::make_shared<FMaterialSemanticRegistry>(*InSemantics);
	Registry->Freeze();
	Semantics = std::move(Registry);
	std::set<std::string> Usages;
	for (FMaterialPass& Pass : Description.Passes)
	{
		ValidatePass(Pass);
		if (!Usages.insert(Pass.Usage).second)
		{
			throw std::invalid_argument("Duplicate material usage: " + Pass.Usage);
		}
	}
	for (FMaterialParameterDeclaration& Parameter : Description.Parameters)
	{
		if (!Parameter.Semantic.empty())
		{
			const FMaterialSemantic& Semantic = Semantics->Find(Parameter.Semantic);
			if (Semantic.Type != Parameter.Type)
			{
				throw std::invalid_argument("Material semantic type mismatch: " + Parameter.Name);
			}
			Parameter.Semantic = Semantic.Name;
		}
	}
	Schema = std::make_shared<FMaterialParameterSchema>(Description.Parameters, Description.Version, false);
}

std::uint64_t FMaterialDefinition::GetIdentity() const
{
	return Identity;
}

const FMaterialDescription& FMaterialDefinition::GetDescription() const
{
	return Description;
}

const FMaterialSemanticRegistry& FMaterialDefinition::GetSemantics() const
{
	return *Semantics;
}

const std::shared_ptr<const FMaterialParameterSchema>& FMaterialDefinition::GetSchema() const
{
	return Schema;
}

bool FMaterialDefinition::HasPass(std::string_view InUsage) const
{
	return std::any_of(Description.Passes.begin(), Description.Passes.end(),
	                   [InUsage](const FMaterialPass& InPass)
	                   {
		                   return InPass.Usage == InUsage;
	                   });
}

const FMaterialPass& FMaterialDefinition::GetPass(std::string_view InUsage) const
{
	for (const FMaterialPass& Pass : Description.Passes)
	{
		if (Pass.Usage == InUsage)
		{
			return Pass;
		}
	}
	throw std::invalid_argument("Material has no usage: " + std::string(InUsage));
}
} // namespace Hyperion
