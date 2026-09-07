#pragma once
#include "Hyperion/Renderer/MaterialGpuCache.h"
#include <algorithm>
#include <map>
#include <set>
#include <thread>

namespace Hyperion
{
struct FMaterialCacheOwnership
{
	struct FUseLess
	{
		// This exact alias name is required by the standard heterogeneous lookup protocol.
		using is_transparent = void; // NOLINT(readability-identifier-naming)

		template<typename TLeft, typename TRight> bool operator()(const TLeft& InA, const TRight& InB) const
		{
			return std::lexicographical_compare(InA.begin(), InA.end(), InB.begin(), InB.end(),
			                                    std::owner_less<void>{});
		}
	};

	std::set<std::vector<std::weak_ptr<const void>>, FUseLess> Uses;
	void Add(const FMaterialResourceOwners& InOwners);
	bool IsUsed();
};

template<typename TDescription, typename TResource> struct TMaterialGpuEntry
{
	TDescription Description;
	TResource Resource;
	FMaterialCacheOwnership Ownership;
};

struct FMaterialGpuCache::FImpl
{
	using FTextureEntry = TMaterialGpuEntry<std::shared_ptr<const FMaterialTextureSource>, FTexture>;
	using FBufferEntry = TMaterialGpuEntry<std::shared_ptr<const FMaterialReadBufferSource>, FBuffer>;
	using FSamplerEntry = TMaterialGpuEntry<FMaterialSampler, FSampler>;
	using FLayoutEntry = TMaterialGpuEntry<FResourceBindingLayoutDesc, FResourceBindingLayout>;
	using FSetEntry = TMaterialGpuEntry<FResourceBindingSetDesc, FResourceBindingSet>;
	using FPipelineEntry = TMaterialGpuEntry<FPipelineDesc, FPipeline>;
	IRHIDevice& Device;
	std::thread::id Owner = std::this_thread::get_id();
	std::map<std::uint64_t, FTextureEntry> Textures;
	std::map<std::uint64_t, FBufferEntry> Buffers;
	std::vector<FSamplerEntry> Samplers;
	std::multimap<std::size_t, FLayoutEntry> Layouts;
	std::multimap<std::size_t, FSetEntry> Sets;
	std::multimap<std::size_t, FPipelineEntry> Pipelines;
	FMaterialGpuStats Stats;

	explicit FImpl(IRHIDevice& InDevice) : Device(InDevice)
	{
	}

	void CheckOwner() const;
	FTexture Texture(std::shared_ptr<const FMaterialTextureSource> InSource, const FMaterialResourceOwners& InOwners);
	FBuffer Buffer(std::shared_ptr<const FMaterialReadBufferSource> InSource, const FMaterialResourceOwners& InOwners);
	FSampler Sampler(const FMaterialSampler& InSampler, const FMaterialResourceOwners& InOwners);
	FResourceBindingValue Resource(const FMaterialValue& InValue, const FMaterialResourceOwners& InOwners);
	FResourceBindingSet Set(FResourceBindingSetDesc InDescription, const FMaterialResourceOwners& InOwners);
};
} // namespace Hyperion
