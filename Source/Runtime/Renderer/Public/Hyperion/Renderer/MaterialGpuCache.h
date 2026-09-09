#pragma once
#include "Hyperion/RHI/RHIDevice.h"
#include "Hyperion/Renderer/MaterialBindingContext.h"

namespace Hyperion
{
using FMaterialResourceOwners = std::vector<std::shared_ptr<const void>>;

struct FMaterialResourceBindings
{
	FResourceBindingLayout Layout;
	FResourceBindingSet Set;
	bool bReady{};
};

struct FMaterialGpuStats
{
	std::uint64_t TextureUploads{};
	std::uint64_t ReadBufferUploads{};
	std::uint64_t SamplersCreated{};
	std::uint64_t LayoutsCreated{};
	std::uint64_t SetsCreated{};
	std::uint64_t PipelinesCreated{};
	std::uint64_t SetReuses{};
	std::uint64_t PipelineReuses{};
	std::size_t LiveObjects{};
};

// Device-local RHI coordinator storage. Owner groups describe complete independent uses of a cache entry.
// A failed/pending bind retains already-submitted uploads until completion even if every owner is released.
class FMaterialGpuCache
{
public:
	explicit FMaterialGpuCache(IRHIDevice& InDevice);
	~FMaterialGpuCache();
	FMaterialGpuCache(const FMaterialGpuCache&) = delete;
	FMaterialGpuCache& operator=(const FMaterialGpuCache&) = delete;
	FResourceBindingLayout GetLayout(const FResourceBindingLayoutDesc& InDescription,
	                                 const FMaterialResourceOwners& InOwners);
	FTexture GetTexture(std::shared_ptr<const FMaterialTextureSource> InSource,
	                    const FMaterialResourceOwners& InOwners);
	FMaterialResourceBindings BindResources(const FCompiledMaterialPass& InPass,
	                                        std::span<const std::optional<FMaterialValue>> InValues,
	                                        const FMaterialResourceOwners& InOwners, bool bInAllowMissing = false);
	FMaterialResourceBindings BindResources(const FCompiledMaterialPass& InPass,
	                                        std::span<const std::shared_ptr<const FMaterialValue>> InValues,
	                                        const FMaterialResourceOwners& InOwners, bool bInAllowMissing = false);
	FPipeline GetPipeline(const FPipelineDesc& InDescription, const FMaterialResourceOwners& InOwners);
	FMaterialResourceBindings BindResources(const FCompiledMaterialPass& InPass, const FMaterialValueTable& InValues,
	                                        const FMaterialResourceOwners& InOwners, bool bInAllowMissing = false);
	FPipeline GetMaterialPipeline(const FCompiledMaterialPass& InProgram, const FMaterialPass& InPass,
	                              const FResourceBindingLayout& InLayout,
	                              const std::vector<FVertexAttribute>& InAttributes, std::uint32_t InStride,
	                              ERHIPrimitiveTopology InTopology, FGraphicsTarget InTarget, bool bInMirrored,
	                              const FMaterialResourceOwners& InOwners);
	// Returns whether submitted resources still require collection after all relevant CPU users have left.
	bool Collect();
	void ClearOwners();
	bool IsEmpty() const;
	FMaterialGpuStats Statistics() const;

private:
	FMaterialResourceBindings BindResourceValues(const FCompiledMaterialPass& InPass,
	                                             std::span<const FMaterialValue* const> InValues,
	                                             const FMaterialResourceOwners& InOwners, bool bInAllowMissing);

	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
