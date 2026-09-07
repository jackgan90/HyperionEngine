#include "Hyperion/Renderer/MaterialPreparation.h"
#include "Hyperion/Renderer/MaterialBlocks.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
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

struct FInterfaceBuilder
{
	std::vector<FMaterialParameterDeclaration> Parameters;
	std::vector<FMaterialTargetMapping> Mappings;
	std::string Usage;
	std::string Variant;
	const FMaterialSemanticRegistry* Semantics{};

	std::optional<std::size_t> Find(const std::string& InPath) const
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

	std::size_t Bind(const std::string& InPath, FMaterialParameterType InType, bool bInActive)
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

	void BindMember(FMaterialProgramBinding& InBinding, const FShaderMember& InMember, const std::string& InParent,
	                std::uint32_t InOffset = 0, bool bInParentActive = true)
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
};

FMaterialParameterType ResourceType(const FShaderBinding& InBinding)
{
	EMaterialValueKind Kind;
	switch (InBinding.Kind)
	{
		case EBindingKind::Texture:
			if (InBinding.Dimension != EShaderResourceDimension::Texture2D ||
			    InBinding.ResourceScalar != EShaderScalar::Float)
			{
				throw std::invalid_argument("Unsupported material texture dimension or component type: " +
				                            InBinding.Name);
			}
			Kind = EMaterialValueKind::Texture2D;
			break;
		case EBindingKind::StructuredBuffer:
		case EBindingKind::RawBuffer:
			Kind = EMaterialValueKind::ReadBuffer;
			break;
		case EBindingKind::Sampler:
			if (InBinding.bComparison)
			{
				throw std::invalid_argument("Unsupported material comparison sampler: " + InBinding.Name);
			}
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

void BindStage(FInterfaceBuilder& InBuilder, FCompiledMaterialPass& InPass, const FShaderArtifact& InArtifact)
{
	const std::string Stage = InArtifact.Stage == EShaderStage::Vertex ? "Vertex:" : "Pixel:";
	for (FShaderBinding Resource : InArtifact.Bindings)
	{
		if (NormalizeStandardMaterialBlock(Resource))
		{
			for (auto Parameter : GetStandardMaterialBlockParameters(Resource.Name, *InBuilder.Semantics))
			{
				if (!InBuilder.Find(Stage + Parameter.Targets.front()))
				{
					Parameter.bActive = false;
					InBuilder.Parameters.push_back(std::move(Parameter));
				}
			}
		}
		FMaterialProgramBinding Binding;
		Binding.Resource = Resource;
		Binding.Stages = InArtifact.Stage == EShaderStage::Vertex ? 1U : 2U;
		if (Resource.Kind == EBindingKind::UniformBuffer)
		{
			if (Resource.Count != 1 || Resource.ByteSize == 0 || Resource.ByteSize > 65536)
			{
				throw std::invalid_argument("Unsupported material constant buffer count or extent: " + Resource.Name);
			}
			for (const FShaderMember& Member : Resource.Members)
			{
				InBuilder.BindMember(Binding, Member, Stage + Resource.Name);
			}
		}
		else
		{
			Binding.ResourceParameter = InBuilder.Bind(Stage + Resource.Name, ResourceType(Resource), true);
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

FShaderCompileOptions CompileOptions(const FMaterialShader& InShader, const FMaterialVariantRequest& InVariant)
{
	FShaderCompileOptions Result;
	for (const FMaterialShaderDefine& Define : InShader.Defines)
	{
		Result.Defines.push_back({Define.Name, Define.Value});
	}
	Result.Defines.insert(Result.Defines.end(), InVariant.Defines.begin(), InVariant.Defines.end());
	return Result;
}
} // namespace

FCompiledMaterialDefinition CompileMaterialDefinition(FShaderCompiler& InCompiler,
                                                      std::shared_ptr<const FMaterialDefinition> InDefinition,
                                                      EShaderFormat InFormat,
                                                      std::vector<FMaterialVariantRequest> InVariants)
{
	if (!InDefinition)
	{
		throw std::invalid_argument("Material preparation requires a definition");
	}
	if (InVariants.empty())
	{
		for (const FMaterialPass& Pass : InDefinition->GetDescription().Passes)
		{
			InVariants.push_back({Pass.Usage});
		}
	}
	std::sort(InVariants.begin(), InVariants.end(),
	          [](const FMaterialVariantRequest& InA, const FMaterialVariantRequest& InB)
	          {
		          return std::tie(InA.Usage, InA.Name) < std::tie(InB.Usage, InB.Name);
	          });
	std::set<std::pair<std::string, std::string>> Variants;
	FInterfaceBuilder Builder;
	Builder.Semantics = &InDefinition->GetSemantics();
	Builder.Parameters = InDefinition->GetSchema()->GetParameters();
	for (FMaterialParameterDeclaration& Parameter : Builder.Parameters)
	{
		Parameter.bActive = false;
	}
	FCompiledMaterialDefinition Result;
	Result.Interface.Definition = InDefinition;
	Result.Key = "material-v1/" + std::to_string(InDefinition->GetIdentity()) + "/" +
	             std::to_string(InDefinition->GetDescription().Version);
	for (const FMaterialVariantRequest& Variant : InVariants)
	{
		if (Variant.Name.empty() || !Variants.emplace(Variant.Usage, Variant.Name).second)
		{
			throw std::invalid_argument("Duplicate or empty material variant");
		}
		const FMaterialPass& Pass = InDefinition->GetPass(Variant.Usage);
		Builder.Usage = Variant.Usage;
		Builder.Variant = Variant.Name;
		FCompiledMaterialPass Compiled;
		Compiled.Usage = Variant.Usage;
		Compiled.Variant = Variant.Name;
		Compiled.Vertex = InCompiler.Compile(Pass.Vertex.Path, Pass.Vertex.Entry, EShaderStage::Vertex, InFormat,
		                                     CompileOptions(Pass.Vertex, Variant));
		BindStage(Builder, Compiled, Compiled.Vertex);
		if (!Pass.Pixel.Path.empty())
		{
			Compiled.Pixel = InCompiler.Compile(Pass.Pixel.Path, Pass.Pixel.Entry, EShaderStage::Pixel, InFormat,
			                                    CompileOptions(Pass.Pixel, Variant));
			BindStage(Builder, Compiled, Compiled.Pixel);
		}
		Compiled.Bindings = MergeMaterialBindings(std::move(Compiled.Bindings));
		std::sort(Compiled.ActiveParameters.begin(), Compiled.ActiveParameters.end());
		Compiled.ActiveParameters.erase(std::unique(Compiled.ActiveParameters.begin(), Compiled.ActiveParameters.end()),
		                                Compiled.ActiveParameters.end());
		Result.Key += "/" + std::to_string(Variant.Usage.size()) + ":" + Variant.Usage +
		              std::to_string(Variant.Name.size()) + ":" + Variant.Name + "/" + Compiled.Vertex.CacheKey + "/" +
		              Compiled.Pixel.CacheKey;
		Result.Passes.push_back(std::move(Compiled));
	}
	// Register reflected aliases only after binding all stages/variants: authored targets drive matching.
	for (const auto& Mapping : Builder.Mappings)
	{
		auto& Targets = Builder.Parameters[Mapping.ParameterIndex].Targets;
		if (std::find(Targets.begin(), Targets.end(), Mapping.Target) == Targets.end())
		{
			Targets.push_back(Mapping.Target);
		}
	}
	Result.Interface.Schema = std::make_shared<const FMaterialParameterSchema>(std::move(Builder.Parameters),
	                                                                           InDefinition->GetDescription().Version);
	Result.Interface.Mappings = std::move(Builder.Mappings);
	return Result;
}

TAsyncResult<FCompiledMaterialDefinition> PrepareMaterialDefinition(
    FTaskSystem& InTasks, std::shared_ptr<FShaderCompiler> InCompiler,
    std::shared_ptr<const FMaterialDefinition> InDefinition, EShaderFormat InFormat,
    std::vector<FMaterialVariantRequest> InVariants, FCancellationToken InCancellation)
{
	InTasks.Require({EDomain::Main});
	if (!InCompiler || !InDefinition)
	{
		throw std::invalid_argument("Material preparation requires owned compiler and definition");
	}
	return DispatchAsync<FCompiledMaterialDefinition>(
	    InTasks, {EDomain::Worker},
	    [Compiler = std::move(InCompiler), Definition = std::move(InDefinition), InFormat,
	     Variants = std::move(InVariants)]
	    {
		    return CompileMaterialDefinition(*Compiler, Definition, InFormat, Variants);
	    },
	    InCancellation);
}
} // namespace Hyperion
