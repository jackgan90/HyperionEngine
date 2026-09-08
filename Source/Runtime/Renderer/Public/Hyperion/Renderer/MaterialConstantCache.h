#pragma once
#include "Hyperion/RHI/RHIDevice.h"
#include "Hyperion/Renderer/MaterialBindingContext.h"

namespace Hyperion
{
struct FMaterialConstantStats
{
	std::uint64_t Packs{};
	std::uint64_t UploadBytes{};
	std::uint64_t Reuses{};
	std::uint64_t PagesCreated{};
	std::uint64_t PagesReset{};
	std::uint64_t FullLookups{};
	std::uint64_t PreparedReuses{};
	std::uint64_t Evictions{};
	std::size_t LivePages{};
	std::size_t CachedBlocks{};
	std::size_t CachedBytes{};
	std::size_t PreparedBlocks{};
	std::size_t PageBytes{};
	std::array<std::uint64_t, MaterialScopeCount> ScopePacks{};
};

struct FMaterialConstantLimits
{
	std::size_t MaxBlocks = 4096;
	std::size_t MaxBytes = 16 * 1024 * 1024;
	std::size_t MaxPreparedBlocks = 512;
};

// Latest numeric state of one prepared draw; never owns scope tokens or an evaluation history.
struct FMaterialConstantState
{
	std::weak_ptr<const FCompiledMaterialDefinition> Program;
	const FCompiledMaterialPass* Pass{};
	FMaterialValueTable Values;
	std::vector<FConstantBinding> Bindings;
};

// RHI coordinator owns this cache. Methods and destruction stay on its construction thread.
// Frozen scope owners control cache retirement; recorded constant slices independently retain native pages.
class FMaterialConstantCache
{
public:
	explicit FMaterialConstantCache(IRHIDevice& InDevice, std::uint32_t InPageSize = 65536,
	                                FMaterialConstantLimits InLimits = {});
	~FMaterialConstantCache();
	FMaterialConstantCache(const FMaterialConstantCache&) = delete;
	FMaterialConstantCache& operator=(const FMaterialConstantCache&) = delete;
	std::vector<FConstantBinding> Bind(const FCompiledMaterialPass& InPass, const FMaterialParameterSchema& InSchema,
	                                   const FResolvedMaterialParameters& InParameters);
	// Call after scope retirement and completed-resource collection; true requests another fence poll.
	// Bind does not scan unrelated cache entries. The resource coordinator schedules collection.
	bool Collect();
	void Clear();
	bool CanRelease() const;
	FMaterialConstantStats Statistics() const;

private:
	friend struct FRenderResourceCoordinator;
	// Only the coordinator can use identities from the engine's immutable preparation pipeline.
	std::vector<FConstantBinding> BindPrepared(std::shared_ptr<const FCompiledMaterialDefinition> InProgram,
	                                           const FCompiledMaterialPass& InPass,
	                                           const FResolvedMaterialParameters& InParameters,
	                                           FMaterialConstantState& InState);
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
