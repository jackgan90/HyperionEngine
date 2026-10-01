#pragma once
#include "Hyperion/Materials/MaterialBlocks.h"

namespace Hyperion
{
enum class EShaderPackingProfile
{
	Uniform,
	Structured
};

void ValidateShaderTextureType(const FMaterialParameterType& InType, std::string_view InName);

// Computes a GPU contract from logical types. C++ object layout is never part of this calculation.
class FShaderParameterLayoutBuilder
{
public:
	explicit FShaderParameterLayoutBuilder(std::string InName, std::string InInstance = {},
	                                       EShaderPackingProfile InProfile = EShaderPackingProfile::Uniform);
	void AddField(std::string InName, const FMaterialParameterType& InType, FMaterialSemanticId InSemantic = {});
	void SetNextOffset(std::uint32_t InOffset);
	void SetMinimumSize(std::uint32_t InSize);
	FStandardMaterialBlock Build() const;

private:
	std::string Name;
	EShaderPackingProfile Profile;
	FStandardMaterialBlock Block;
	std::uint32_t Cursor{};
	std::uint32_t MinimumSize{};
	bool bExplicitOffset{};
};
} // namespace Hyperion
