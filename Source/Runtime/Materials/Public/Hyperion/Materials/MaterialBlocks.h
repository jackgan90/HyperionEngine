#pragma once
#include "Hyperion/Materials/MaterialSemantics.h"

namespace Hyperion
{
struct FStandardMaterialBlockMember
{
	std::string Name;
	FMaterialSemanticId Semantic;
	std::uint32_t Offset{};
	std::uint32_t Columns = 1;
	std::uint32_t Rows = 1;
	EMaterialScalar Scalar = EMaterialScalar::Float;
	bool operator==(const FStandardMaterialBlockMember&) const = default;
};

struct FStandardMaterialBlock
{
	std::uint32_t Size{};
	std::vector<FStandardMaterialBlockMember> Members;
	std::string Instance;
	bool operator==(const FStandardMaterialBlock&) const = default;
};

struct FEngineMaterialResource
{
	FMaterialSemanticId Semantic;
	EMaterialValueKind Kind = EMaterialValueKind::Numeric;
	std::uint32_t StructureByteStride{};
	bool bComparison{};
	FStandardMaterialBlock ElementLayout;
	bool bWritable{};
	bool operator==(const FEngineMaterialResource&) const = default;
};

struct FShaderParameterContractSet
{
	std::string IncludeName;
	std::uint64_t Version = 1;
	std::vector<std::pair<std::string, FStandardMaterialBlock>> Uniforms;
	std::vector<std::pair<std::string, FEngineMaterialResource>> Resources;
	std::vector<std::pair<std::string, std::string>> UniformAliases;
	std::vector<const FMaterialSemantic*> Semantics;
	bool operator==(const FShaderParameterContractSet&) const = default;
};

using FShaderParameterContracts = std::vector<std::shared_ptr<const FShaderParameterContractSet>>;
const FShaderParameterContracts& GetStandardShaderContracts();
FMaterialSemanticId FindStandardShaderSemantic(std::string_view InName);

FEngineMaterialResource GetEngineMaterialResource(
    std::string_view InName, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts = {});
std::uint64_t GetEngineSemanticContractVersion();

FStandardMaterialBlock GetStandardMaterialBlock(
    std::string_view InName, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts = {});
std::vector<FMaterialParameterDeclaration> GetStandardMaterialBlockParameters(
    std::string_view InBlock, const FMaterialSemanticRegistry& InRegistry,
    std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts = {});
} // namespace Hyperion
