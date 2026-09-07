#pragma once
#include "Hyperion/Materials/MaterialParameters.h"

namespace Hyperion
{
struct FMaterialSemantic
{
	std::string Name;
	FMaterialParameterType Type;
	EMaterialScope Scope = EMaterialScope::Material;
	std::string Convention;
	bool bRequired = true;
	std::vector<std::string> Aliases;
	bool operator==(const FMaterialSemantic&) const = default;
};

class FMaterialSemanticRegistry
{
public:
	FMaterialSemanticRegistry();
	void Register(FMaterialSemantic InSemantic);
	void Freeze();
	const FMaterialSemantic& Find(std::string_view InName) const;
	std::string Normalize(std::string_view InName) const;
	std::uint64_t GetVersion() const;

private:
	void Add(FMaterialSemantic InSemantic);
	std::vector<FMaterialSemantic> Semantics;
	std::uint64_t Version = 1;
	bool bFrozen{};
};

std::shared_ptr<const FMaterialSemanticRegistry> GetStandardMaterialSemantics();
FMaterialParameterDeclaration DeclareMaterialSemantic(std::string InName, std::string_view InSemantic,
                                                      const FMaterialSemanticRegistry& InRegistry);
} // namespace Hyperion
