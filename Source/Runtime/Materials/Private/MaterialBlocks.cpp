#include "Hyperion/Materials/MaterialBlocks.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "ShaderContracts.h"

namespace Hyperion
{
std::uint64_t GetEngineSemanticContractVersion()
{
	return GetEngineShaderContracts()->Version;
}

FStandardMaterialBlock GetStandardMaterialBlock(
    std::string_view InName, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	const auto Local = FindShaderUniformContract(InName, InContracts);
	if (Local.Size != 0)
	{
		return Local;
	}
	const auto& Common = *GetEngineShaderContracts();
	for (const auto& Alias : Common.UniformAliases)
	{
		if (InName == Alias.first)
		{
			InName = Alias.second;
			break;
		}
	}
	for (const auto& Uniform : Common.Uniforms)
	{
		if (InName == Uniform.first)
		{
			return Uniform.second;
		}
	}
	return {};
}

FEngineMaterialResource GetEngineMaterialResource(
    std::string_view InName, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	return FindShaderResourceContract(InName, InContracts);
}

std::vector<FMaterialParameterDeclaration> GetStandardMaterialBlockParameters(
    std::string_view InBlock, const FMaterialSemanticRegistry& InRegistry,
    std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	std::vector<FMaterialParameterDeclaration> Result;
	for (const auto& Member : GetStandardMaterialBlock(InBlock, InContracts).Members)
	{
		auto Parameter = DeclareMaterialSemantic(std::string(Member.Semantic.GetName()), Member.Semantic, InRegistry);
		Parameter.Targets = {std::string(InBlock) + "." + std::string(Member.Name)};
		Result.push_back(std::move(Parameter));
	}
	return Result;
}
} // namespace Hyperion
