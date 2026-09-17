#include "InstancePacking.h"
#include "Hyperion/Renderer/MaterialPacking.h"
#include "SceneItemPreparation.h"
#include <stdexcept>

namespace Hyperion
{
bool FInstanceRecordLayout::Matches(const FInstanceRecordLayout& InOther) const
{
	return Format == InOther.Format && Stride == InOther.Stride && Members == InOther.Members &&
	       Mapping == InOther.Mapping;
}

std::shared_ptr<const FCompiledMaterialDefinition> GetInstanceProgram(const FRenderItem& InItem)
{
	const auto Program = InItem.Preparation && InItem.Preparation->Program
	                         ? InItem.Preparation->Program
	                         : (InItem.State.Surface ? InItem.State.Surface->GetCompiled() : nullptr);
	if (!Program)
	{
		throw std::invalid_argument("Instance packing requires a ready material program");
	}
	return Program;
}

FInstanceRecordLayout DescribeInstanceRecordLayout(const FCompiledMaterialDefinition& InProgram,
                                                   const FCompiledMaterialPass& InPass,
                                                   const FMaterialProgramBinding& InBinding)
{
	FInstanceRecordLayout Result;
	Result.Format = InPass.Vertex.Format;
	Result.Stride = InBinding.InstanceStride;
	for (const auto& Member : InBinding.Members)
	{
		Result.Members.push_back(Member.Layout);
		const auto& Parameter = InProgram.Interface.Schema->GetParameters().at(Member.ParameterIndex);
		Result.Mapping.emplace_back(Parameter.Name, Parameter.Semantic);
	}
	return Result;
}

std::vector<std::byte> PackInstanceRecord(const FRenderItem& InItem, const FMaterialProgramBinding& InBinding)
{
	if (!InItem.ResolvedParameters)
	{
		throw std::invalid_argument("Instance packing requires resolved material parameters");
	}
	const auto Parameters = InItem.SharedParameters
	                            ? ComposeMaterialParameters(*InItem.ResolvedParameters, *InItem.SharedParameters)
	                            : *InItem.ResolvedParameters;
	return PackMaterialConstants(InBinding, Parameters.Values);
}
} // namespace Hyperion
