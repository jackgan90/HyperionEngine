#include "Hyperion/Renderer/MaterialBindingContext.h"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace Hyperion
{
namespace
{
void ApplyDependencyOverrides(FResolvedMaterialParameters& InResult, const FMaterialParameterSchema& InSchema,
                              const FMaterialParameterValues& InValues, EMaterialScope InScope)
{
	for (const auto& Entry : InValues)
	{
		InResult.Dependencies[InSchema.Find(Entry.Name).Index] = MaterialScopeBit(InScope);
	}
}

std::map<std::string, const FMaterialProvidedValue*> SelectProviders(const FMaterialSnapshot& InSnapshot,
                                                                     const FMaterialBindingContext& InContext,
                                                                     FMaterialParameterValues& OutValues)
{
	std::map<std::string, const FMaterialProvidedValue*> Result;
	for (const auto& Provider : InContext.Providers)
	{
		// A session can serve shaders with different extension registries. Unused providers are irrelevant.
		const auto& Parameters = InSnapshot.Schema->GetParameters();
		const bool bUsed = std::any_of(Parameters.begin(), Parameters.end(),
		                               [&](const FMaterialParameterDeclaration& InParameter)
		                               {
			                               return InParameter.Source == EMaterialParameterSource::Semantic &&
			                                      InParameter.Semantic == Provider.Semantic;
		                               });
		if (!bUsed)
		{
			continue;
		}
		const auto& Semantic = InSnapshot.Definition->GetSemantics().Find(Provider.Semantic);
		if (Provider.Dependencies == 0 || (Provider.Dependencies >> MaterialScopeCount) != 0 ||
		    (Provider.Dependencies & MaterialScopeBit(Semantic.Scope)) == 0 ||
		    !Result.emplace(Semantic.Name, &Provider).second)
		{
			throw std::invalid_argument("Conflicting material provider or incomplete scope dependency: " +
			                            Semantic.Name);
		}
		if (Provider.Value)
		{
			OutValues.push_back({Semantic.Name, *Provider.Value});
		}
	}
	return Result;
}
} // namespace

FResolvedMaterialParameters ResolveMaterialBindingContext(std::shared_ptr<const FMaterialSnapshot> InMaterial,
                                                          const FCompiledMaterialDefinition& InCompiled,
                                                          const FCompiledMaterialPass& InPass,
                                                          const FMaterialBindingContext& InContext)
{
	if (!InMaterial || InMaterial->Definition != InCompiled.Interface.Definition || !InCompiled.Interface.Schema)
	{
		throw std::invalid_argument("Material snapshot and compiled definition are incompatible");
	}
	// Declared-only snapshots are valid for asynchronous imported materials; bind against the prepared schema.
	FMaterialSnapshot Snapshot = *InMaterial;
	Snapshot.Schema = InCompiled.Interface.Schema;
	FMaterialParameterValues ProviderValues;
	const auto Providers = SelectProviders(Snapshot, InContext, ProviderValues);
	const auto Values = ResolveMaterialParameters(Snapshot, ProviderValues, InContext.ObjectParameters,
	                                              InContext.DrawParameters, InPass.ActiveParameters);
	FResolvedMaterialParameters Result;
	Result.Scopes = InContext.Scopes;
	Result.Scopes[static_cast<std::size_t>(EMaterialScope::Material)] = {{Snapshot.Identity, Snapshot.Revision},
	                                                                     InMaterial};
	const auto& Parameters = Snapshot.Schema->GetParameters();
	Result.Values.resize(Parameters.size());
	Result.Dependencies.resize(Parameters.size(), MaterialScopeBit(EMaterialScope::Material));
	for (const auto& Value : Values)
	{
		Result.Values[Snapshot.Schema->Find(Value.Name).Index] = Value.Value;
	}
	for (std::size_t Index = 0; Index < Parameters.size(); ++Index)
	{
		const auto& Parameter = Parameters[Index];
		const auto Provider = Providers.find(Parameter.Semantic);
		if (Parameter.Source == EMaterialParameterSource::Semantic)
		{
			const auto Dependencies =
			    Provider != Providers.end()
			        ? Provider->second->Dependencies
			        : MaterialScopeBit(Snapshot.Definition->GetSemantics().Find(Parameter.Semantic).Scope);
			Result.Dependencies[Index] = Dependencies;
			if (Provider == Providers.end() || !Provider->second->Value)
			{
				Result.Dependencies[Index] |= MaterialScopeBit(EMaterialScope::Material);
			}
		}
	}
	ApplyDependencyOverrides(Result, *Snapshot.Schema, Snapshot.Overrides, EMaterialScope::Material);
	ApplyDependencyOverrides(Result, *Snapshot.Schema, InContext.ObjectParameters, EMaterialScope::Object);
	ApplyDependencyOverrides(Result, *Snapshot.Schema, InContext.DrawParameters, EMaterialScope::Draw);
	return Result;
}
} // namespace Hyperion
