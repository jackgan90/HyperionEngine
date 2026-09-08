#pragma once
#include "Hyperion/Renderer/MaterialBindingContext.h"
#include "Hyperion/Renderer/MaterialInputValues.h"
#include <functional>

namespace Hyperion
{
struct FMaterialProviderInputs
{
	std::array<FMaterialScopeInput, MaterialScopeCount> Scopes;
	std::array<FMaterialInputValues, MaterialScopeCount> Values;
	const FMaterialValue* Find(EMaterialScope InScope, std::string_view InName) const;
};

struct FMaterialProviderDescription
{
	std::string Semantic;
	std::uint32_t Dependencies{};
	std::function<std::optional<FMaterialValue>(const FMaterialProviderInputs&)> Evaluate;
};

struct FMaterialProviderStats
{
	std::array<std::uint64_t, MaterialScopeCount> Evaluations{};
	std::uint64_t Reuses{};
	std::uint64_t CachedEntries{};
	std::uint64_t CachedValueBytes{};
	std::uint64_t Evictions{};
};

struct FMaterialProviderLimits
{
	std::size_t MaxEntries = 4096;
	std::size_t MaxValueBytes = 16 * 1024 * 1024;
};

// Configure on Main before Freeze. Thereafter Evaluate/Collect belong to the Render preparation task,
// before native materialization. Providers receive only owned CPU inputs, never an RHI callback.
class FMaterialProviderRegistry
{
public:
	explicit FMaterialProviderRegistry(
	    std::shared_ptr<const FMaterialSemanticRegistry> InSemantics = GetStandardMaterialSemantics(),
	    FMaterialProviderLimits InLimits = {});
	~FMaterialProviderRegistry();
	FMaterialProviderRegistry(const FMaterialProviderRegistry&) = delete;
	FMaterialProviderRegistry& operator=(const FMaterialProviderRegistry&) = delete;
	void Register(FMaterialProviderDescription InDescription);
	void Freeze();
	std::uint64_t GetVersion() const;
	std::vector<FMaterialProvidedValue> Evaluate(const FMaterialProviderInputs& InInputs,
	                                             std::span<const std::string> InSemantics);
	FMaterialProvidedValue EvaluateOne(const FMaterialProviderInputs& InInputs, std::string_view InSemantic);
	void Collect();
	FMaterialProviderStats Statistics() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
