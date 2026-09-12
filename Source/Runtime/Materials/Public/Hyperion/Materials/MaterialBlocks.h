#pragma once
#include "Hyperion/Materials/MaterialSemantics.h"

namespace Hyperion
{
struct FStandardMaterialBlockMember
{
	std::string_view Name;
	std::string_view Semantic;
	std::uint32_t Offset{};
	std::uint32_t Columns = 1;
	std::uint32_t Rows = 1;
	EMaterialScalar Scalar = EMaterialScalar::Float;
};

struct FStandardMaterialBlock
{
	std::uint32_t Size{};
	std::vector<FStandardMaterialBlockMember> Members;
};

FStandardMaterialBlock GetStandardMaterialBlock(std::string_view InName);
std::vector<FMaterialParameterDeclaration> GetStandardMaterialBlockParameters(
    std::string_view InBlock, const FMaterialSemanticRegistry& InRegistry);
} // namespace Hyperion
