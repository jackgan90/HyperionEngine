#pragma once
#include "Hyperion/Renderer/MaterialPreparation.h"
#include <array>

namespace Hyperion
{
inline constexpr std::size_t MaterialScopeCount = static_cast<std::size_t>(EMaterialScope::Count);

struct FMaterialScopeKey
{
	std::uint64_t Identity{};
	std::uint64_t Revision{};
	// Object/Draw keys include primitive generation, local item identity and collection qualifiers.
	std::vector<std::uint64_t> Qualifiers;
	bool operator==(const FMaterialScopeKey&) const = default;
};

struct FMaterialScopeInput
{
	FMaterialScopeKey Key;
	// The cache does not keep a scope alive. Its producer retains this token for the scope's useful lifetime.
	std::shared_ptr<const void> Lifetime;
};

struct FMaterialProvidedValue
{
	std::string Semantic;
	std::optional<FMaterialValue> Value; // An absent result still carries its invalidation dependencies.
	std::uint32_t Dependencies{};
};

struct FMaterialBindingContext
{
	std::array<FMaterialScopeInput, MaterialScopeCount> Scopes;
	std::vector<FMaterialProvidedValue> Providers;
	FMaterialParameterValues ObjectParameters;
	FMaterialParameterValues DrawParameters;
};

struct FResolvedMaterialParameters
{
	// Frozen values are shared individually; a camera update copies handles, not every material array.
	std::vector<std::shared_ptr<const FMaterialValue>> Values;
	std::vector<std::uint32_t> Dependencies;
	std::array<FMaterialScopeInput, MaterialScopeCount> Scopes;
	// Shared only while resource values and their dependency owners are unchanged across numeric refreshes.
	std::shared_ptr<const void> ResourceIdentity;
};

inline bool SameMaterialValue(const std::shared_ptr<const FMaterialValue>& InA,
                              const std::shared_ptr<const FMaterialValue>& InB)
{
	return InA == InB || (InA && InB && *InA == *InB);
}

FResolvedMaterialParameters ResolveMaterialBindingContext(std::shared_ptr<const FMaterialSnapshot> InMaterial,
                                                          const FCompiledMaterialDefinition& InCompiled,
                                                          const FCompiledMaterialPass& InPass,
                                                          const FMaterialBindingContext& InContext);
} // namespace Hyperion
