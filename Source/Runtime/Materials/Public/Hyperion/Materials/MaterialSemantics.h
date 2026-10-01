#pragma once
#include "Hyperion/Materials/MaterialParameters.h"

namespace Hyperion
{
enum class EEngineSemanticGroup
{
	General,
	Pbr,
	Shadow,
	Environment
};
enum class EMaterialEditHint
{
	Default,
	Color,
	HdrColor,
	UvSet
};

struct FEngineSemanticPolicy
{
	EEngineSemanticGroup Group = EEngineSemanticGroup::General;
	bool bSceneOwned{};
	bool bAllowDefaultSceneInput{};
	EMaterialEditHint EditHint = EMaterialEditHint::Default;
	bool operator==(const FEngineSemanticPolicy&) const = default;
};

FEngineSemanticPolicy GetEngineSemanticPolicy(FMaterialSemanticId InSemantic);

struct FMaterialSemantic
{
	std::string Name;
	FMaterialParameterType Type;
	EMaterialScope Scope = EMaterialScope::Material;
	std::string Convention;
	bool bRequired = true;
	std::vector<std::string> Aliases;
	FEngineSemanticPolicy Policy;
	bool operator==(const FMaterialSemantic&) const = default;
};

class FMaterialSemanticRegistry
{
public:
	FMaterialSemanticRegistry();
	void Register(FMaterialSemantic InSemantic);
	void Freeze();
	const FMaterialSemantic& Find(FMaterialSemanticId InName) const;
	FMaterialSemanticId Normalize(FMaterialSemanticId InName) const;

	bool IsFrozen() const
	{
		return bFrozen;
	}

	std::uint64_t GetVersion() const;

private:
	void Add(FMaterialSemantic InSemantic);
	std::shared_ptr<const std::vector<FMaterialSemantic>> Builtins;
	std::vector<FMaterialSemantic> Semantics;
	std::uint64_t Version = 1;
	bool bFrozen{};
};

std::shared_ptr<const FMaterialSemanticRegistry> GetStandardMaterialSemantics();
FMaterialParameterDeclaration DeclareMaterialSemantic(std::string InName, FMaterialSemanticId InSemantic,
                                                      const FMaterialSemanticRegistry& InRegistry);
} // namespace Hyperion
