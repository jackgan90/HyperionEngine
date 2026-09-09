#pragma once
#include "Hyperion/Renderer/MaterialParameterTable.h"
#include "Hyperion/Renderer/MaterialPreparation.h"
#include <array>

namespace Hyperion
{
inline constexpr std::size_t MaterialScopeCount = static_cast<std::size_t>(EMaterialScope::Count);

struct FMaterialScopeKey
{
	FMaterialScopeKey() = default;

	FMaterialScopeKey(std::uint64_t InIdentity, std::uint64_t InRevision, std::vector<std::uint64_t> InQualifiers = {})
	    : Identity(InIdentity), Revision(InRevision)
	{
		SetQualifiers(std::move(InQualifiers));
	}

	std::uint64_t Identity{};
	std::uint64_t Revision{};

	// Object/Draw keys include primitive generation, local item identity and collection qualifiers.
	void SetQualifiers(std::vector<std::uint64_t> InQualifiers)
	{
		Qualifiers = InQualifiers.empty() ? nullptr
		                                  : std::make_shared<const std::vector<std::uint64_t>>(std::move(InQualifiers));
	}

	const std::vector<std::uint64_t>& GetQualifiers() const
	{
		static const std::vector<std::uint64_t> Empty;
		return Qualifiers ? *Qualifiers : Empty;
	}

	bool operator==(const FMaterialScopeKey& InOther) const
	{
		return Identity == InOther.Identity && Revision == InOther.Revision &&
		       (Qualifiers == InOther.Qualifiers || GetQualifiers() == InOther.GetQualifiers());
	}

private:
	std::shared_ptr<const std::vector<std::uint64_t>> Qualifiers;
};

struct FMaterialScopeInput
{
	FMaterialScopeKey Key;
	// The cache does not keep a scope alive. Its producer retains this token for the scope's useful lifetime.
	std::shared_ptr<const void> Lifetime;
};

class FMaterialSharedValue
{
public:
	FMaterialSharedValue() = default;

	FMaterialSharedValue(FMaterialValue InValue) : Value(std::make_shared<const FMaterialValue>(std::move(InValue)))
	{
	}

	FMaterialSharedValue(std::optional<FMaterialValue> InValue)
	    : Value(InValue ? std::make_shared<const FMaterialValue>(std::move(*InValue)) : nullptr)
	{
	}

	explicit operator bool() const
	{
		return bool(Value);
	}

	const FMaterialValue& operator*() const
	{
		return *Value;
	}

	const FMaterialValue* operator->() const
	{
		return Value.get();
	}

	const std::shared_ptr<const FMaterialValue>& Share() const
	{
		return Value;
	}

	bool operator==(const FMaterialSharedValue& InOther) const
	{
		return Value == InOther.Value || (Value && InOther.Value && *Value == *InOther.Value);
	}

	bool operator==(const FMaterialValue& InOther) const
	{
		return Value && *Value == InOther;
	}

private:
	std::shared_ptr<const FMaterialValue> Value;
};

struct FMaterialProvidedValue
{
	std::string Semantic;
	FMaterialSharedValue Value; // An absent result still carries its invalidation dependencies.
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
	FMaterialValueTable Values;
	FMaterialDependencyTable Dependencies;
	std::uint32_t DependenciesMask{};
	std::array<FMaterialScopeInput, MaterialScopeCount> Scopes;
	// Owns a resource-value epoch across numeric refreshes; replaced when a resource value changes.
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
