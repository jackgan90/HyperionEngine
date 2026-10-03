#pragma once
#include "Hyperion/Renderer/MaterialPreparation.h"

namespace Hyperion
{
// Accumulates a shared logical interface while each variant retains its reflected native binding layout.
class FMaterialInterfaceBuilder
{
public:
	FMaterialInterfaceBuilder(std::shared_ptr<const FMaterialDefinition> InDefinition,
	                          EMaterialEngineBindingMode InEngineMode);
	std::span<const std::shared_ptr<const FShaderParameterContractSet>> GetContracts() const;
	void BindStage(FCompiledMaterialPass& InPass, const FShaderArtifact& InArtifact,
	               const FMaterialPass& InDescription);
	FPreparedMaterialInterface Finish();

private:
	void AddEngineParameter(FMaterialParameterDeclaration InParameter);
	std::optional<std::size_t> Find(const std::string& InPath) const;
	std::size_t Bind(const std::string& InPath, FMaterialParameterType InType, bool bInActive);
	void BindMember(FMaterialProgramBinding& InBinding, const FShaderMember& InMember, const std::string& InParent,
	                std::uint32_t InOffset = 0, bool bInParentActive = true);

	std::shared_ptr<const FMaterialDefinition> Definition;
	std::vector<FMaterialParameterDeclaration> Parameters;
	std::vector<FMaterialTargetMapping> Mappings;
	std::string Usage;
	std::string Variant;
	const FMaterialSemanticRegistry* Semantics{};
	std::span<const std::shared_ptr<const FShaderParameterContractSet>> Contracts;
	EMaterialEngineBindingMode EngineMode;
};
} // namespace Hyperion
