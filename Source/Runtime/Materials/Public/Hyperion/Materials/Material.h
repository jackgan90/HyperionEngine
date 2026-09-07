#pragma once
#include "Hyperion/Materials/MaterialSemantics.h"
#include "Hyperion/Materials/MaterialState.h"
#include <thread>

namespace Hyperion
{
struct FMaterialDescription
{
	std::string Name;
	std::uint64_t Version = 1;
	std::vector<FMaterialPass> Passes;
	std::vector<FMaterialParameterDeclaration> Parameters;
};

class FMaterialDefinition
{
public:
	explicit FMaterialDefinition(
	    FMaterialDescription InDescription,
	    std::shared_ptr<const FMaterialSemanticRegistry> InSemantics = GetStandardMaterialSemantics());
	FMaterialDefinition(const FMaterialDefinition&) = delete;
	FMaterialDefinition& operator=(const FMaterialDefinition&) = delete;
	std::uint64_t GetIdentity() const;
	const FMaterialDescription& GetDescription() const;
	const FMaterialSemanticRegistry& GetSemantics() const;
	const std::shared_ptr<const FMaterialParameterSchema>& GetSchema() const;
	bool HasPass(std::string_view InUsage) const;
	const FMaterialPass& GetPass(std::string_view InUsage = "Forward") const;

private:
	std::uint64_t Identity;
	FMaterialDescription Description;
	std::shared_ptr<const FMaterialSemanticRegistry> Semantics;
	std::shared_ptr<const FMaterialParameterSchema> Schema;
};

struct FMaterialTargetMapping
{
	std::string Usage;
	std::string Variant;
	std::string Target;
	std::size_t ParameterIndex{};
	bool bActive = true;
};

// Renderer publishes this CPU-only interface; compiled/native artifacts stay with their owners.
struct FPreparedMaterialInterface
{
	std::shared_ptr<const FMaterialDefinition> Definition;
	std::shared_ptr<const FMaterialParameterSchema> Schema;
	std::vector<FMaterialTargetMapping> Mappings;
};

struct FMaterialSnapshot
{
	std::uint64_t Identity{};
	std::uint64_t Revision{};
	std::shared_ptr<const FMaterialDefinition> Definition;
	std::shared_ptr<const FMaterialParameterSchema> Schema;
	FMaterialParameterValues Overrides;
};

enum class EMaterialWriteResult : std::uint8_t
{
	Active,
	Inactive
};

// Construction and all instance access belong to one Main owner. Only Freeze results cross domains.
class FMaterialInstance
{
public:
	explicit FMaterialInstance(std::shared_ptr<const FMaterialDefinition> InDefinition);
	explicit FMaterialInstance(const FPreparedMaterialInterface& InInterface);
	FMaterialInstance(const FMaterialInstance&) = delete;
	FMaterialInstance& operator=(const FMaterialInstance&) = delete;
	std::uint64_t GetIdentity() const;
	std::uint64_t GetRevision() const;
	std::shared_ptr<const FMaterialSnapshot> Freeze() const;
	FMaterialParameterHandle Find(std::string_view InName) const;
	EMaterialWriteResult Set(std::string_view InName, FMaterialValue InValue);
	EMaterialWriteResult Set(FMaterialParameterHandle InHandle, FMaterialValue InValue);
	EMaterialWriteResult SetSemantic(std::string_view InSemantic, FMaterialValue InValue);
	void Clear(std::string_view InName);
	void ReplaceDefinition(const FPreparedMaterialInterface& InInterface);

private:
	void CheckOwner() const;
	void Publish(FMaterialSnapshot InSnapshot);
	std::thread::id Owner;
	std::shared_ptr<const FMaterialSnapshot> Snapshot;
};

// Provider entries use normalized semantic names. Override entries use schema parameter names/aliases.
FMaterialParameterValues ResolveMaterialParameters(const FMaterialSnapshot& InSnapshot,
                                                   const FMaterialParameterValues& InProviders,
                                                   const FMaterialParameterValues& InObject = {},
                                                   const FMaterialParameterValues& InDraw = {},
                                                   std::optional<std::span<const std::size_t>> InActiveParameters = {});
} // namespace Hyperion
