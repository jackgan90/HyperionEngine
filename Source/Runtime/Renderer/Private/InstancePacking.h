#pragma once
#include "Hyperion/Renderer/MaterialPreparation.h"

namespace Hyperion
{
struct FRenderItem;

// The byte contract is independent of program identity and cache ownership.
struct FInstanceRecordLayout
{
	EShaderFormat Format{};
	std::uint32_t Stride{};
	std::vector<FShaderMember> Members;
	std::vector<std::pair<std::string, std::string>> Mapping;

	bool Matches(const FInstanceRecordLayout& InOther) const;
};

std::shared_ptr<const FCompiledMaterialDefinition> GetInstanceProgram(const FRenderItem& InItem);
FInstanceRecordLayout DescribeInstanceRecordLayout(const FCompiledMaterialDefinition& InProgram,
                                                   const FCompiledMaterialPass& InPass,
                                                   const FMaterialProgramBinding& InBinding);
std::vector<std::byte> PackInstanceRecord(const FRenderItem& InItem, const FMaterialProgramBinding& InBinding);
} // namespace Hyperion
