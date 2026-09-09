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

// Local Object/Material/Draw scopes and shared engine scopes have independent immutable ownership.
// Camera refreshes replace one shared input block without copying every object's scope keys and tokens.
class FMaterialResolvedScopes
{
public:
	using FValues = std::array<FMaterialScopeInput, MaterialScopeCount>;
	static constexpr std::uint32_t EngineMask =
	    MaterialScopeBit(EMaterialScope::Global) | MaterialScopeBit(EMaterialScope::Frame) |
	    MaterialScopeBit(EMaterialScope::Scene) | MaterialScopeBit(EMaterialScope::View) |
	    MaterialScopeBit(EMaterialScope::Pass);

	FMaterialResolvedScopes& operator=(const FValues& InValues)
	{
		Local = std::make_shared<FValues>(InValues);
		Engine.reset();
		return *this;
	}

	const FMaterialScopeInput& operator[](std::size_t InIndex) const
	{
		if (InIndex >= MaterialScopeCount)
		{
			throw std::out_of_range("Material scope index");
		}
		if (Engine && (EngineMask & (1U << InIndex)))
		{
			return (*Engine)[InIndex];
		}
		static const FMaterialScopeInput Empty;
		return Local ? (*Local)[InIndex] : Empty;
	}

	void Set(std::size_t InIndex, FMaterialScopeInput InValue)
	{
		if (InIndex >= MaterialScopeCount || (Engine && (EngineMask & (1U << InIndex))))
		{
			throw std::invalid_argument("Shared material engine scopes require a complete input block");
		}
		MakeLocal();
		(*Local)[InIndex] = std::move(InValue);
	}

	void ShareEngine(std::shared_ptr<const FValues> InEngine)
	{
		if (!InEngine)
		{
			throw std::invalid_argument("Missing shared material engine scopes");
		}
		if (!Engine)
		{
			MakeLocal();
			for (std::size_t Index = 0; Index < MaterialScopeCount; ++Index)
			{
				if (EngineMask & (1U << Index))
				{
					(*Local)[Index] = {};
				}
			}
		}
		Engine = std::move(InEngine);
	}

private:
	std::shared_ptr<FValues> Local;
	std::shared_ptr<const FValues> Engine;

	void MakeLocal()
	{
		if (!Local || Local.use_count() != 1)
		{
			Local = Local ? std::make_shared<FValues>(*Local) : std::make_shared<FValues>();
		}
	}
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
	FMaterialResolvedScopes Scopes;
	// Owns a resource-value epoch across numeric refreshes; replaced when a resource value changes.
	std::shared_ptr<const void> ResourceIdentity;
	std::uint32_t LocalDependenciesMask{};
};

// One immutable update shared by compatible items; local evaluation keeps its identity across camera changes.
struct FMaterialSharedParameters
{
	std::shared_ptr<const FMaterialValueTable::FSharedValues> Values;
	std::shared_ptr<const FMaterialDependencyTable::FSharedValues> Dependencies;
	std::shared_ptr<const FMaterialResolvedScopes::FValues> Scopes;
	std::uint32_t DependenciesMask{};
};

FResolvedMaterialParameters ComposeMaterialParameters(const FResolvedMaterialParameters& InLocal,
                                                      const FMaterialSharedParameters& InShared);

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
