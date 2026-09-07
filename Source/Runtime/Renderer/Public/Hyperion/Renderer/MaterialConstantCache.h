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
	std::size_t LivePages{};
	std::size_t CachedBlocks{};
	std::array<std::uint64_t, MaterialScopeCount> ScopePacks{};
};

// RHI coordinator owns this cache. Methods and destruction stay on its construction thread.
// Frozen scope owners control cache retirement; recorded constant slices independently retain native pages.
class FMaterialConstantCache
{
public:
	explicit FMaterialConstantCache(IRHIDevice& InDevice, std::uint32_t InPageSize = 65536);
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
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
