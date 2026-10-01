#include "ShaderContracts.h"
#include "Hyperion/Materials/Lighting/ClusterParameters.h"
#include "Hyperion/Materials/Lighting/EnvironmentParameters.h"
#include "Hyperion/Materials/Lighting/SceneLightingParameters.h"
#include "Hyperion/Materials/Lighting/ShadowParameters.h"
#include "Hyperion/Materials/MaterialBlocks.h"
#include "Hyperion/Materials/ObjectParameters.h"
#include "Hyperion/Materials/PbrParameters.h"
#include <algorithm>
#include <map>
#include <set>

namespace Hyperion
{
void ValidateShaderContracts(std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	std::set<std::string> Names;
	for (const auto& Uniform : GetEngineShaderContracts()->Uniforms)
	{
		Names.insert(Uniform.first);
	}
	for (const auto& Alias : GetEngineShaderContracts()->UniformAliases)
	{
		Names.insert(Alias.first);
	}
	std::map<std::string, const FShaderParameterContractSet*> Includes;
	for (const auto Sets :
	     {std::span<const std::shared_ptr<const FShaderParameterContractSet>>(GetStandardShaderContracts()),
	      InContracts})
	{
		for (const auto& Set : Sets)
		{
			if (!Set || Set->Version == 0 || Set->IncludeName.empty() ||
			    Set->IncludeName == "HyperionUniforms.generated.hlsli")
			{
				throw std::invalid_argument("Invalid owner shader contract set");
			}
			const auto [Existing, bInserted] = Includes.emplace(Set->IncludeName, Set.get());
			if (!bInserted)
			{
				if (*Existing->second != *Set)
				{
					throw std::invalid_argument("Conflicting shader contract include: " + Set->IncludeName);
				}
				continue;
			}
			const auto AddName = [&](const std::string& InName)
			{
				if (InName.empty() || !Names.insert(InName).second)
				{
					throw std::invalid_argument("Conflicting shader contract resource: " + InName);
				}
			};
			for (const auto& Uniform : Set->Uniforms)
			{
				AddName(Uniform.first);
			}
			for (const auto& Resource : Set->Resources)
			{
				AddName(Resource.first);
			}
			for (const auto& Alias : Set->UniformAliases)
			{
				AddName(Alias.first);
			}
		}
	}
}

const FShaderParameterContracts& GetStandardShaderContracts()
{
	static const FShaderParameterContracts Contracts{
	    GetObjectShaderContracts(), GetPbrShaderContracts(),     GetSceneLightingShaderContracts(),
	    GetShadowShaderContracts(), GetClusterShaderContracts(), GetEnvironmentShaderContracts()};
	return Contracts;
}

FMaterialSemanticId FindStandardShaderSemantic(std::string_view InName)
{
	for (const auto& Set : GetStandardShaderContracts())
	{
		for (const auto* Semantic : Set->Semantics)
		{
			if (Semantic->Name == InName ||
			    std::find(Semantic->Aliases.begin(), Semantic->Aliases.end(), InName) != Semantic->Aliases.end())
			{
				return FMaterialSemanticId(Semantic);
			}
		}
	}
	return {};
}

FStandardMaterialBlock FindShaderUniformContract(
    std::string_view InName, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	for (const auto Sets :
	     {std::span<const std::shared_ptr<const FShaderParameterContractSet>>(GetStandardShaderContracts()),
	      InContracts})
	{
		for (const auto& Set : Sets)
		{
			std::string_view Name = InName;
			for (const auto& Alias : Set->UniformAliases)
			{
				if (Alias.first == Name)
				{
					Name = Alias.second;
					break;
				}
			}
			for (const auto& Uniform : Set->Uniforms)
			{
				if (Uniform.first == Name)
				{
					return Uniform.second;
				}
			}
		}
	}
	return {};
}

FEngineMaterialResource FindShaderResourceContract(
    std::string_view InName, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	for (const auto Sets :
	     {std::span<const std::shared_ptr<const FShaderParameterContractSet>>(GetStandardShaderContracts()),
	      InContracts})
	{
		for (const auto& Set : Sets)
		{
			for (const auto& Resource : Set->Resources)
			{
				if (Resource.first == InName)
				{
					return Resource.second;
				}
			}
		}
	}
	return {};
}
} // namespace Hyperion
