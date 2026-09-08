#include "Hyperion/Renderer/MaterialPreparation.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
FMaterialParameterType GetMaterialParameterType(const FShaderMember& InMember)
{
	if (InMember.Kind == EShaderValueKind::Array)
	{
		if (InMember.Members.size() != 1)
		{
			throw std::invalid_argument("Invalid reflected material array");
		}
		return FMaterialParameterType::Array(GetMaterialParameterType(InMember.Members.front()), InMember.ArrayCount);
	}
	if (InMember.Kind == EShaderValueKind::Structure)
	{
		FMaterialParameterType Result;
		Result.Kind = EMaterialValueKind::Structure;
		for (const FShaderMember& Member : InMember.Members)
		{
			Result.MemberNames.push_back(Member.Name);
			Result.Members.push_back(GetMaterialParameterType(Member));
		}
		Result.Validate();
		return Result;
	}
	EMaterialScalar Scalar;
	switch (InMember.Scalar)
	{
		case EShaderScalar::Bool:
			Scalar = EMaterialScalar::Bool;
			break;
		case EShaderScalar::Int:
			Scalar = EMaterialScalar::Int;
			break;
		case EShaderScalar::Uint:
			Scalar = EMaterialScalar::Uint;
			break;
		case EShaderScalar::Float:
			Scalar = EMaterialScalar::Float;
			break;
		default:
			throw std::invalid_argument("Unsupported material scalar type: " + InMember.Name);
	}
	return FMaterialParameterType::Numeric(Scalar, InMember.Columns, InMember.Rows);
}

namespace
{
FShaderMember CanonicalLayout(FShaderMember InMember)
{
	InMember.Name.clear();
	InMember.bActive = true;
	for (FShaderMember& Member : InMember.Members)
	{
		Member = CanonicalLayout(std::move(Member));
	}
	return InMember;
}

bool Compatible(const FMaterialProgramBinding& InA, const FMaterialProgramBinding& InB)
{
	if (InA.Resource.Kind != InB.Resource.Kind || InA.Resource.Register != InB.Resource.Register ||
	    InA.Resource.Count != InB.Resource.Count || InA.Resource.ByteSize != InB.Resource.ByteSize ||
	    InA.Resource.Dimension != InB.Resource.Dimension ||
	    InA.Resource.ResourceScalar != InB.Resource.ResourceScalar ||
	    InA.Resource.StructureByteStride != InB.Resource.StructureByteStride ||
	    InA.Resource.bComparison != InB.Resource.bComparison || InA.ResourceParameter != InB.ResourceParameter ||
	    InA.InstanceStride != InB.InstanceStride || InA.InstanceCapacity != InB.InstanceCapacity ||
	    InA.Members.size() != InB.Members.size())
	{
		return false;
	}
	for (std::size_t Index = 0; Index < InA.Members.size(); ++Index)
	{
		if (InA.Members[Index].ParameterIndex != InB.Members[Index].ParameterIndex ||
		    CanonicalLayout(InA.Members[Index].Layout) != CanonicalLayout(InB.Members[Index].Layout))
		{
			return false;
		}
	}
	return true;
}

std::uint32_t RegisterClass(EBindingKind InKind)
{
	if (InKind == EBindingKind::UniformBuffer)
	{
		return 0;
	}
	if (InKind == EBindingKind::Sampler)
	{
		return 2;
	}
	return 1;
}
} // namespace

std::vector<FMaterialProgramBinding> MergeMaterialBindings(std::vector<FMaterialProgramBinding> InBindings)
{
	std::vector<FMaterialProgramBinding> Result;
	for (FMaterialProgramBinding& Binding : InBindings)
	{
		if (Binding.Stages == 0 || (Binding.Stages & ~3U) != 0 || Binding.Resource.Count == 0)
		{
			throw std::invalid_argument("Invalid material binding visibility or count");
		}
		FMaterialProgramBinding* MergeTarget = nullptr;
		for (FMaterialProgramBinding& Existing : Result)
		{
			if (Existing.Resource.Space != Binding.Resource.Space ||
			    RegisterClass(Existing.Resource.Kind) != RegisterClass(Binding.Resource.Kind) ||
			    std::uint64_t(Existing.Resource.Register) + Existing.Resource.Count <= Binding.Resource.Register ||
			    std::uint64_t(Binding.Resource.Register) + Binding.Resource.Count <= Existing.Resource.Register)
			{
				continue;
			}
			if (Compatible(Existing, Binding))
			{
				MergeTarget = &Existing;
				continue;
			}
			if ((Existing.Stages & Binding.Stages) != 0)
			{
				throw std::invalid_argument("Overlapping incompatible material binding: " + Existing.Resource.Name +
				                            " / " + Binding.Resource.Name);
			}
		}
		if (MergeTarget)
		{
			MergeTarget->Stages |= Binding.Stages;
		}
		else
		{
			Result.push_back(std::move(Binding));
		}
	}
	return Result;
}

const FCompiledMaterialPass* FCompiledMaterialDefinition::FindInstancePass(std::string_view InUsage) const
{
	for (const auto& Pass : Passes)
	{
		if (Pass.Usage == InUsage && Pass.Variant == "Instance")
		{
			return &Pass;
		}
	}
	return nullptr;
}

const FCompiledMaterialPass& FCompiledMaterialDefinition::GetPass(std::string_view InUsage,
                                                                  std::string_view InVariant) const
{
	for (const FCompiledMaterialPass& Pass : Passes)
	{
		if (Pass.Usage == InUsage && Pass.Variant == InVariant)
		{
			return Pass;
		}
	}
	throw std::invalid_argument("Compiled material usage/variant is unavailable");
}
} // namespace Hyperion
