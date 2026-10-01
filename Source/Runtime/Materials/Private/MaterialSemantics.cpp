#include "Hyperion/Materials/MaterialSemantics.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
FEngineSemanticPolicy GetEngineSemanticPolicy(FMaterialSemanticId InSemantic)
{
	if (InSemantic.GetDescriptor())
	{
		return InSemantic.GetDescriptor()->Policy;
	}
	if (InSemantic.GetBuiltin() != EEngineSemantic::None)
	{
		return GetStandardMaterialSemantics()->Find(InSemantic).Policy;
	}
	return {};
}

namespace
{
void ValidateSemantic(const FMaterialSemantic& InSemantic, std::span<const FMaterialSemantic> InBuiltins,
                      std::span<const FMaterialSemantic> InCustom)
{
	InSemantic.Type.Validate();
	if (InSemantic.Name.empty() || InSemantic.Convention.empty() || InSemantic.Scope >= EMaterialScope::Count)
	{
		throw std::invalid_argument("Invalid material semantic contract");
	}
	std::vector<std::string> Names = InSemantic.Aliases;
	Names.push_back(InSemantic.Name);
	std::sort(Names.begin(), Names.end());
	if (Names.front().empty() || std::adjacent_find(Names.begin(), Names.end()) != Names.end())
	{
		throw std::invalid_argument("Duplicate or empty material semantic alias");
	}
	for (const auto ExistingSet : {InBuiltins, InCustom})
	{
		for (const FMaterialSemantic& Existing : ExistingSet)
		{
			for (const std::string& Name : Names)
			{
				if (Existing.Name == Name ||
				    std::find(Existing.Aliases.begin(), Existing.Aliases.end(), Name) != Existing.Aliases.end())
				{
					throw std::invalid_argument("Conflicting material semantic registration: " + Name);
				}
			}
		}
	}
}

std::shared_ptr<const std::vector<FMaterialSemantic>> BuiltinCatalog()
{
	static const auto Catalog = []
	{
		std::vector<FMaterialSemantic> Values = GetEngineShaderSemantics();

		for (std::size_t Index = 0; Index < Values.size(); ++Index)
		{
			ValidateSemantic(Values[Index], std::span<const FMaterialSemantic>(Values).first(Index), {});
		}
		return std::make_shared<const std::vector<FMaterialSemantic>>(std::move(Values));
	}();
	return Catalog;
}
} // namespace

FMaterialSemanticRegistry::FMaterialSemanticRegistry() : Builtins(BuiltinCatalog())
{
}

void FMaterialSemanticRegistry::Add(FMaterialSemantic InSemantic)
{
	ValidateSemantic(InSemantic, *Builtins, Semantics);
	Semantics.push_back(std::move(InSemantic));
}

void FMaterialSemanticRegistry::Register(FMaterialSemantic InSemantic)
{
	if (bFrozen || InSemantic.Name.find('.') == std::string::npos || InSemantic.Name.starts_with("Engine.") ||
	    InSemantic.Name.starts_with("Pbr."))
	{
		throw std::invalid_argument("Material semantics must be registered before freeze in a custom namespace");
	}
	for (const std::string& Alias : InSemantic.Aliases)
	{
		if (Alias.find('.') == std::string::npos || Alias.starts_with("Engine.") || Alias.starts_with("Pbr."))
		{
			throw std::invalid_argument("Custom semantic aliases require a custom namespace");
		}
	}
	Add(std::move(InSemantic));
	++Version;
}

void FMaterialSemanticRegistry::Freeze()
{
	bFrozen = true;
}

std::uint64_t FMaterialSemanticRegistry::GetVersion() const
{
	return Version;
}

const FMaterialSemantic& FMaterialSemanticRegistry::Find(FMaterialSemanticId InName) const
{
	if (InName.GetDescriptor())
	{
		return *InName.GetDescriptor();
	}
	if (InName.GetBuiltin() != EEngineSemantic::None)
	{
		return Builtins->at(static_cast<std::size_t>(InName.GetBuiltin()) - 1);
	}
	for (const FMaterialSemantic& Semantic : Semantics)
	{
		if (Semantic.Name == InName ||
		    std::find(Semantic.Aliases.begin(), Semantic.Aliases.end(), InName) != Semantic.Aliases.end())
		{
			return Semantic;
		}
	}
	throw std::invalid_argument("Unknown material semantic: " + std::string(InName.GetName()));
}

FMaterialSemanticId FMaterialSemanticRegistry::Normalize(FMaterialSemanticId InName) const
{
	if (InName.IsBuiltin())
	{
		return InName;
	}
	return Find(InName).Name;
}

FMaterialParameterDeclaration DeclareMaterialSemantic(std::string InName, FMaterialSemanticId InSemantic,
                                                      const FMaterialSemanticRegistry& InRegistry)
{
	const FMaterialSemantic& Semantic = InRegistry.Find(InSemantic);
	FMaterialParameterDeclaration Result;
	Result.Name = std::move(InName);
	Result.Type = Semantic.Type;
	Result.Semantic = InRegistry.Normalize(InSemantic);
	Result.bRequired = Semantic.bRequired;
	if (GetEngineSemanticPolicy(InRegistry.Normalize(InSemantic)).Group != EEngineSemanticGroup::Pbr)
	{
		Result.Source = EMaterialParameterSource::Semantic;
		Result.OverridePolicy = EMaterialOverridePolicy::Locked;
	}
	return Result;
}

std::shared_ptr<const FMaterialSemanticRegistry> GetStandardMaterialSemantics()
{
	static const std::shared_ptr<const FMaterialSemanticRegistry> Registry = []
	{
		auto Result = std::make_shared<FMaterialSemanticRegistry>();
		Result->Freeze();
		return Result;
	}();
	return Registry;
}
} // namespace Hyperion
