#include "MaterialInterfaceBuilder.h"
#include "EnvironmentParameters.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "Hyperion/Renderer/CascadedShadowMap.h"
#include "Hyperion/Renderer/ClusteredLights.h"
#include "Hyperion/Renderer/MaterialBlocks.h"
#include "Hyperion/Renderer/MaterialInputValues.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{
void CompleteEngineDefaults(FMaterialParameterDeclaration& InParameter)
{
	if (InParameter.Source != EMaterialParameterSource::Semantic || InParameter.Default ||
	    !InParameter.Semantic.IsBuiltin())
	{
		return;
	}
	static const FMaterialInputValues Defaults = []
	{
		auto Values = DefaultShadowParameters();
		for (const auto& Inputs : {DefaultClusterParameters(), EnvironmentParameters()})
		{
			Values.insert(Values.end(), Inputs.begin(), Inputs.end());
		}
		return FMaterialInputValues(std::move(Values));
	}();
	if (const auto* Value = Defaults.Find(InParameter.Semantic))
	{
		InParameter.Default = *Value;
	}
}

FShaderMember NestArrayLayout(const FMaterialParameterType& InType, const FShaderMember& InLeaf,
                              std::uint32_t InLeafStride)
{
	if (InType.Kind != EMaterialValueKind::Array)
	{
		return InLeaf;
	}
	FShaderMember Result;
	Result.Kind = EShaderValueKind::Array;
	Result.ArrayCount = InType.ArrayCount;
	Result.Members.push_back(NestArrayLayout(InType.Members.front(), InLeaf, InLeafStride));
	const auto& Element = Result.Members.front();
	const std::uint64_t Stride = Element.Kind == EShaderValueKind::Array
	                                 ? std::uint64_t(Element.ArrayStride) * Element.ArrayCount
	                                 : InLeafStride;
	const std::uint64_t Size = (Result.ArrayCount - 1) * Stride + Element.Size;
	if (Stride > 65536 || Size > 65536)
	{
		throw std::invalid_argument("Material nested array exceeds constant buffer range");
	}
	Result.ArrayStride = static_cast<std::uint32_t>(Stride);
	Result.Size = static_cast<std::uint32_t>(Size);
	return Result;
}

FShaderMember MatchAuthoredArrayLayout(FShaderMember InLayout, const FMaterialParameterType& InType)
{
	if (InType.Kind == EMaterialValueKind::Structure && InLayout.Kind == EShaderValueKind::Structure &&
	    InType.Members.size() == InLayout.Members.size())
	{
		for (std::size_t Index = 0; Index < InLayout.Members.size(); ++Index)
		{
			if (InType.MemberNames[Index] != InLayout.Members[Index].Name)
			{
				return InLayout;
			}
			InLayout.Members[Index] =
			    MatchAuthoredArrayLayout(std::move(InLayout.Members[Index]), InType.Members[Index]);
		}
	}
	if (InType.Kind != EMaterialValueKind::Array || InLayout.Kind != EShaderValueKind::Array ||
	    InLayout.Members.size() != 1)
	{
		return InLayout;
	}
	if (InLayout.Members.front().Kind == EShaderValueKind::Array ||
	    InType.Members.front().Kind != EMaterialValueKind::Array)
	{
		InLayout.Members.front() =
		    MatchAuthoredArrayLayout(std::move(InLayout.Members.front()), InType.Members.front());
		return InLayout;
	}
	const auto* LeafType = &InType;
	std::uint64_t Count = 1;
	while (LeafType->Kind == EMaterialValueKind::Array)
	{
		Count *= LeafType->ArrayCount;
		if (Count > 65536)
		{
			throw std::invalid_argument("Material nested array element count exceeds supported range");
		}
		LeafType = &LeafType->Members.front();
	}
	const auto Leaf = MatchAuthoredArrayLayout(InLayout.Members.front(), *LeafType);
	if (Count != InLayout.ArrayCount || GetMaterialParameterType(Leaf) != *LeafType)
	{
		return InLayout; // Bind reports the complete type conflict; byte size alone is never sufficient.
	}
	// DXIL exposes total element count. The author supplies dimensions; preserve every native leaf address.
	auto Result = NestArrayLayout(InType, Leaf, InLayout.ArrayStride);
	if (Result.Size > InLayout.Size)
	{
		throw std::invalid_argument("Material nested array layout exceeds reflected extent");
	}
	Result.Name = InLayout.Name;
	Result.Offset = InLayout.Offset;
	Result.Size = InLayout.Size;
	Result.bActive = InLayout.bActive;
	return Result;
}

FMaterialParameterType ResourceType(const FShaderBinding& InBinding)
{
	EMaterialValueKind Kind;
	switch (InBinding.Kind)
	{
		case EBindingKind::Texture:
			if ((InBinding.Dimension != EShaderResourceDimension::Texture2D &&
			     InBinding.Dimension != EShaderResourceDimension::TextureCube) ||
			    InBinding.ResourceScalar != EShaderScalar::Float)
			{
				throw std::invalid_argument("Unsupported material texture dimension or component type: " +
				                            InBinding.Name);
			}
			Kind = InBinding.Dimension == EShaderResourceDimension::TextureCube ? EMaterialValueKind::TextureCube
			                                                                    : EMaterialValueKind::Texture2D;
			break;
		case EBindingKind::StructuredBuffer:
		case EBindingKind::RawBuffer:
			Kind = EMaterialValueKind::ReadBuffer;
			break;
		case EBindingKind::Sampler:
			Kind = EMaterialValueKind::Sampler;
			break;
		default:
			throw std::invalid_argument("Unsupported material resource kind: " + InBinding.Name);
	}
	FMaterialParameterType Result = FMaterialParameterType::Resource(Kind);
	if (InBinding.Count > 1)
	{
		Result = FMaterialParameterType::Array(std::move(Result), InBinding.Count);
	}
	return Result;
}

} // namespace

FMaterialInterfaceBuilder::FMaterialInterfaceBuilder(std::shared_ptr<const FMaterialDefinition> InDefinition,
                                                     EMaterialEngineBindingMode InEngineMode)
    : Definition(std::move(InDefinition)), Semantics(&Definition->GetSemantics()),
      Contracts(Definition->GetDescription().ShaderContracts), EngineMode(InEngineMode)
{
	Parameters = Definition->GetSchema()->GetParameters();
	for (auto& Parameter : Parameters)
	{
		CompleteEngineDefaults(Parameter);
		Parameter.bActive = false;
	}
}

std::span<const std::shared_ptr<const FShaderParameterContractSet>> FMaterialInterfaceBuilder::GetContracts() const
{
	return Contracts;
}

FPreparedMaterialInterface FMaterialInterfaceBuilder::Finish()
{
	// Register reflected aliases only after binding all stages/variants: authored targets drive matching.
	for (const auto& Mapping : Mappings)
	{
		auto& Targets = Parameters[Mapping.ParameterIndex].Targets;
		if (std::find(Targets.begin(), Targets.end(), Mapping.Target) == Targets.end())
		{
			Targets.push_back(Mapping.Target);
		}
	}
	return {
	    Definition,
	    std::make_shared<const FMaterialParameterSchema>(std::move(Parameters), Definition->GetDescription().Version),
	    std::move(Mappings)};
}

void FMaterialInterfaceBuilder::AddEngineParameter(FMaterialParameterDeclaration InParameter)
{
	const auto Existing =
	    std::find_if(Parameters.begin(), Parameters.end(),
	                 [&](const auto& InExisting)
	                 {
		                 return InExisting.Name == InParameter.Name && InExisting.Semantic == InParameter.Semantic;
	                 });
	if (Existing != Parameters.end())
	{
		Existing->Targets.insert(Existing->Targets.end(), InParameter.Targets.begin(), InParameter.Targets.end());
		return;
	}
	if (EngineMode == EMaterialEngineBindingMode::Explicit)
	{
		InParameter.Source = EMaterialParameterSource::Manual;
		InParameter.OverridePolicy = EMaterialOverridePolicy::AllowOverride;
	}
	CompleteEngineDefaults(InParameter);
	InParameter.bActive = false;
	Parameters.push_back(std::move(InParameter));
}

std::optional<std::size_t> FMaterialInterfaceBuilder::Find(const std::string& InPath) const
{
	std::optional<std::size_t> Result;
	const std::string Unqualified = InPath.substr(InPath.find(':') + 1);
	const std::string Leaf = InPath.substr(InPath.find_last_of(".:") + 1);
	for (std::size_t Index = 0; Index < Parameters.size(); ++Index)
	{
		const FMaterialParameterDeclaration& Parameter = Parameters[Index];
		const bool bMatches =
		    Parameter.Name == InPath ||
		    (Parameter.Targets.empty() && (Parameter.Name == Unqualified || Parameter.Name == Leaf)) ||
		    std::any_of(Parameter.Targets.begin(), Parameter.Targets.end(),
		                [&](const std::string& InTarget)
		                {
			                return InTarget == InPath || InTarget == Unqualified;
		                });
		if (bMatches)
		{
			if (Result)
			{
				throw std::invalid_argument("Ambiguous material schema target: " + InPath);
			}
			Result = Index;
		}
	}
	return Result;
}

std::size_t FMaterialInterfaceBuilder::Bind(const std::string& InPath, FMaterialParameterType InType, bool bInActive)
{
	std::optional<std::size_t> Index = Find(InPath);
	if (!Index)
	{
		FMaterialParameterDeclaration Parameter;
		Parameter.Name = InPath;
		Parameter.Type = std::move(InType);
		Parameter.bActive = false;
		Index = Parameters.size();
		Parameters.push_back(std::move(Parameter));
	}
	else if (Parameters[*Index].Type != InType)
	{
		throw std::invalid_argument("Material schema/variant type conflict: " + InPath);
	}
	Parameters[*Index].bActive |= bInActive;
	Mappings.push_back({Usage, Variant, InPath, *Index, bInActive});
	return *Index;
}

void FMaterialInterfaceBuilder::BindMember(FMaterialProgramBinding& InBinding, const FShaderMember& InMember,
                                           const std::string& InParent, std::uint32_t InOffset, bool bInParentActive)
{
	const std::string Path = InParent + "." + InMember.Name;
	const bool bActive = bInParentActive && InMember.bActive;
	const std::optional<std::size_t> Existing = Find(Path);
	if (InMember.Kind == EShaderValueKind::Structure && !Existing)
	{
		for (const FShaderMember& Member : InMember.Members)
		{
			BindMember(InBinding, Member, Path, InOffset + InMember.Offset, bActive);
		}
		return;
	}
	if (!bActive && !Existing)
	{
		return;
	}
	FShaderMember Layout = Existing ? MatchAuthoredArrayLayout(InMember, Parameters[*Existing].Type) : InMember;
	const std::size_t Index = Bind(Path, GetMaterialParameterType(Layout), bActive);
	if (bActive)
	{
		Layout.Offset += InOffset;
		InBinding.Members.push_back({std::move(Layout), Index});
	}
}

void FMaterialInterfaceBuilder::BindStage(FCompiledMaterialPass& InPass, const FShaderArtifact& InArtifact,
                                          const FMaterialPass& InDescription)
{
	Usage = InPass.Usage;
	Variant = InPass.Variant;
	const std::string Stage = InArtifact.Stage == EShaderStage::Vertex ? "Vertex:" : "Pixel:";
	for (const FShaderBinding& NativeResource : InArtifact.Bindings)
	{
		FMaterialProgramBinding Binding;
		auto Resource = InPass.ExecutionMode == EMaterialExecutionMode::Instanced
		                    ? PrepareMaterialInstanceBinding(NativeResource, InDescription, Binding)
		                    : NativeResource;
		if (NormalizeStandardMaterialBlock(Resource, Contracts))
		{
			for (auto Parameter : GetStandardMaterialBlockParameters(Resource.Name, *Semantics, Contracts))
			{
				const auto& Contract = GetStandardMaterialBlock(Resource.Name, Contracts);
				const auto Member = std::find_if(Contract.Members.begin(), Contract.Members.end(),
				                                 [&](const auto& InMember)
				                                 {
					                                 return InMember.Semantic == Parameter.Semantic;
				                                 });
				if (Member == Contract.Members.end() || std::none_of(Resource.Members.begin(), Resource.Members.end(),
				                                                     [&](const auto& InMember)
				                                                     {
					                                                     return InMember.Name == Member->Name;
				                                                     }))
				{
					continue;
				}
				if (!Find(Stage + Parameter.Targets.front()))
				{
					AddEngineParameter(std::move(Parameter));
				}
			}
		}
		if (!Binding.InstanceStride)
		{
			const auto Contract = ValidateEngineMaterialResource(Resource, Contracts);
			if (!Contract.Semantic.IsEmpty() && !Find(Stage + Resource.Name))
			{
				auto Parameter =
				    DeclareMaterialSemantic(std::string(Contract.Semantic.GetName()), Contract.Semantic, *Semantics);
				Parameter.Targets = {Resource.Name};
				AddEngineParameter(std::move(Parameter));
			}
			Binding.Resource = Resource;
		}
		Binding.Stages = ShaderStageMask(InArtifact.Stage);
		if (Resource.Kind == EBindingKind::UniformBuffer)
		{
			if (Resource.Count != 1 || Resource.ByteSize == 0 || Resource.ByteSize > 65536)
			{
				throw std::invalid_argument("Unsupported material constant buffer count or extent: " + Resource.Name);
			}
			for (const FShaderMember& Member : Resource.Members)
			{
				BindMember(Binding, Member, Stage + Resource.Name);
			}
		}
		else
		{
			Binding.ResourceParameter = Bind(Stage + Resource.Name, ResourceType(Resource), true);
		}
		for (const FMaterialBindingMember& Member : Binding.Members)
		{
			InPass.ActiveParameters.push_back(Member.ParameterIndex);
		}
		if (Binding.ResourceParameter)
		{
			InPass.ActiveParameters.push_back(*Binding.ResourceParameter);
		}
		InPass.Bindings.push_back(std::move(Binding));
	}
}

} // namespace Hyperion
