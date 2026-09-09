#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Hyperion/Renderer/RenderBatch.h"
#include "Hyperion/Renderer/RenderResources.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace Hyperion
{
namespace
{
void ResourceKey(const FMaterialValue& InValue, std::vector<std::uint64_t>& OutWords)
{
	OutWords.push_back(static_cast<unsigned>(InValue.Type.Kind));
	switch (InValue.Type.Kind)
	{
		case EMaterialValueKind::Array:
			OutWords.push_back(InValue.Elements.size());
			for (const auto& Element : InValue.Elements)
			{
				ResourceKey(Element, OutWords);
			}
			break;
		case EMaterialValueKind::Texture2D:
			OutWords.push_back(InValue.Texture->GetIdentity());
			OutWords.push_back(InValue.Texture->GetVersion());
			OutWords.push_back(static_cast<unsigned>(InValue.Texture->GetEncoding()));
			break;
		case EMaterialValueKind::ReadBuffer:
			OutWords.insert(OutWords.end(), {InValue.Buffer.Source->GetIdentity(), InValue.Buffer.Source->GetVersion(),
			                                 static_cast<unsigned>(InValue.Buffer.Kind), InValue.Buffer.Offset,
			                                 InValue.Buffer.Size, InValue.Buffer.Stride});
			break;
		case EMaterialValueKind::Sampler:
		{
			const auto& Sampler = InValue.Sampler;
			OutWords.insert(OutWords.end(), {static_cast<unsigned>(Sampler.U), static_cast<unsigned>(Sampler.V),
			                                 static_cast<unsigned>(Sampler.W), Sampler.bMinLinear, Sampler.bMagLinear,
			                                 Sampler.bMipLinear, Sampler.bComparison, Sampler.MaxAnisotropy,
			                                 static_cast<unsigned>(Sampler.Compare)});
			for (const auto Value : {Sampler.MipLodBias, Sampler.MinLod, Sampler.MaxLod, Sampler.BorderColor[0],
			                         Sampler.BorderColor[1], Sampler.BorderColor[2], Sampler.BorderColor[3]})
			{
				OutWords.push_back(std::bit_cast<std::uint32_t>(Value));
			}
			break;
		}
		default:
			throw std::invalid_argument("Non-resource value in batch resource signature");
	}
}
} // namespace

bool FRenderBatchSignature::operator==(const FRenderBatchSignature& InOther) const
{
	if (GeometryIdentity != InOther.GeometryIdentity || GeometryIndex != InOther.GeometryIndex ||
	    FirstIndex != InOther.FirstIndex || IndexCount != InOther.IndexCount || VertexStride != InOther.VertexStride ||
	    Topology != InOther.Topology || Attributes != InOther.Attributes || VertexProgram != InOther.VertexProgram ||
	    PixelProgram != InOther.PixelProgram || State != InOther.State || DynamicState != InOther.DynamicState ||
	    Target != InOther.Target || Layout != InOther.Layout || Resources != InOther.Resources ||
	    SharedValues.size() != InOther.SharedValues.size())
	{
		return false;
	}
	for (std::size_t Index = 0; Index < SharedValues.size(); ++Index)
	{
		if (!SameMaterialValue(SharedValues[Index], InOther.SharedValues[Index]))
		{
			return false;
		}
	}
	return true;
}

std::size_t FRenderBatchSignature::Hash() const
{
	std::size_t Hash = GeometryIdentity;
	for (const auto Value : {GeometryIndex, FirstIndex, IndexCount, VertexStride})
	{
		Hash = Hash * 16777619U ^ Value;
	}
	Hash ^= std::hash<std::string>{}(VertexProgram);
	Hash = Hash * 16777619U ^ std::hash<std::string>{}(PixelProgram);
	for (const auto Value : Resources)
	{
		Hash = Hash * 16777619U ^ Value;
	}
	return Hash; // Full equality also checks state, shared values and complete layout after every hash hit.
}

FRenderBatchCandidate DescribeBatchCandidate(const FRenderItem& InItem, const FRenderView& InView,
                                             FGraphicsTarget InTarget)
{
	if (!InItem.State.Resource || !InItem.State.Surface || !InItem.ResolvedParameters)
	{
		throw std::invalid_argument("Batch candidate requires resolved geometry and material");
	}
	FRenderBatchCandidate Result;
	Result.Program = InItem.State.Surface->GetCompiled();
	Result.Pass = Result.Program->FindInstancePass(InView.Usage);
	if (!Result.Pass)
	{
		Result.Pass = &Result.Program->GetPass(InView.Usage);
	}
	const auto& Pass = InItem.State.Surface->GetSnapshot()->Definition->GetPass(InView.Usage);
	const auto Description = InItem.State.Resource->GetDescription();
	const auto& Section = Description->Sections.at(InItem.State.Section);
	const auto& Geometry = Description->Geometries.at(Section.Geometry);
	auto& Signature = Result.Signature;
	Signature.GeometryIdentity = InItem.State.Resource->GetIdentity();
	Signature.GeometryIndex = Section.Geometry;
	Signature.FirstIndex = Section.FirstIndex;
	Signature.IndexCount = Section.IndexCount;
	Signature.VertexStride = Geometry.VertexStride;
	Signature.Topology = Geometry.Topology;
	Signature.Attributes = Geometry.Attributes;
	Signature.VertexProgram = Result.Pass->Vertex.CacheKey;
	Signature.PixelProgram = Result.Pass->Pixel.CacheKey;
	Signature.State = ConvertMaterialState(Pass.State, Determinant(InItem.State.World) < 0);
	Signature.DynamicState = ConvertMaterialDynamicState(InItem.DynamicState.value_or(Pass.DynamicState));
	Signature.Target = InTarget;
	Signature.Layout = DescribeMaterialLayout(*Result.Pass);
	Result.bReorderable = Pass.bAllowBatchReordering &&
	                      (Pass.Queue == EMaterialQueue::Opaque || Pass.Queue == EMaterialQueue::Masked) &&
	                      !Pass.State.bBlend && !Pass.State.bStencil && Pass.State.bDepthTest && Pass.State.bDepthWrite;
	for (const auto& Binding : Result.Pass->Bindings)
	{
		if (Binding.ResourceParameter)
		{
			ResourceKey(*InItem.ResolvedParameters->Values[*Binding.ResourceParameter], Signature.Resources);
		}
		else if (!Binding.InstanceStride)
		{
			for (const auto& Member : Binding.Members)
			{
				Signature.SharedValues.push_back(InItem.ResolvedParameters->Values[Member.ParameterIndex]);
			}
		}
	}
	Result.CompatibilityHash = Result.Signature.Hash();
	return Result;
}

FRenderBatchDecision FInstanceBatchStrategy::Evaluate(const FRenderBatchCandidate& InCandidate,
                                                      const FRHICapabilities& InCapabilities) const
{
	if (!InCapabilities.QueryFeature(ERHIFeature::InstancedDrawing).bEnabled)
	{
		return {0, ERenderBatchFallback::Device};
	}
	if (!InCandidate.Pass || InCandidate.Pass->InstanceCapacity < 2)
	{
		return {0, ERenderBatchFallback::Shader};
	}
	if (!InCandidate.bReorderable)
	{
		return {0, ERenderBatchFallback::Order};
	}
	auto Capacity = InCandidate.Pass->InstanceCapacity;
	std::array<std::uint32_t, 2> Constants{};
	for (const auto& Binding : InCandidate.Pass->Bindings)
	{
		if (Binding.Resource.Kind == EBindingKind::UniformBuffer)
		{
			if (Binding.Resource.ByteSize > InCapabilities.MaxConstantRange)
			{
				return {0, ERenderBatchFallback::Device};
			}
			for (std::size_t Stage = 0; Stage < Constants.size(); ++Stage)
			{
				Constants[Stage] += (Binding.Stages & (1U << Stage)) ? Binding.Resource.Count : 0;
				if (Constants[Stage] > InCapabilities.MaxConstantBuffers)
				{
					return {0, ERenderBatchFallback::Device};
				}
			}
		}
		if (Binding.InstanceStride)
		{
			Capacity = std::min(Capacity, InCapabilities.MaxConstantRange / Binding.InstanceStride);
		}
	}
	return {Capacity >= 2 ? Capacity : 0, ERenderBatchFallback::Device};
}

bool FInstanceBatchStrategy::CanCombine(const FRenderBatchCandidate& InA, const FRenderBatchCandidate& InB) const
{
	return InA.Signature == InB.Signature;
}

std::string_view GetRenderBatchFallbackName(ERenderBatchFallback InReason)
{
	constexpr std::array Names{"disabled", "shader", "device", "ordering", "singleton", "preparation"};
	const auto Index = static_cast<std::size_t>(InReason);
	return Index < Names.size() ? Names[Index] : "unknown";
}
} // namespace Hyperion
