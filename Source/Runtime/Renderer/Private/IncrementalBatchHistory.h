#pragma once
#include "Hyperion/Renderer/RenderBatch.h"
#include <map>
#include <tuple>
#include <unordered_map>

namespace Hyperion
{
using FIncrementalSourceKey = std::tuple<std::uint64_t, std::uint32_t, std::uint64_t, std::uint64_t>;

struct FIncrementalSourceKeyHash
{
	std::size_t operator()(const FIncrementalSourceKey& InKey) const;
};

// Only compatibility metadata and weak source proofs persist here. Instance payload ownership stays
// in the existing byte-budgeted chunk cache; earlier plans own their independently immutable packets.
struct FIncrementalBatchHistory
{
	struct FSource
	{
		std::weak_ptr<const FSceneItemPreparation> Preparation;
		std::weak_ptr<const FResolvedMaterialParameters> Parameters;
		std::size_t Block{};
		std::size_t Index{};
		std::uint64_t Access{};
	};

	struct FBlock
	{
		std::vector<FIncrementalSourceKey> Members;
		std::vector<std::size_t> Indices;
		std::size_t Group{};
		std::shared_ptr<const void> Contents;
		std::optional<FIncrementalSourceKey> Chunk;
		bool bDirty{};
		bool bRemap{};
	};

	struct FGroup
	{
		std::shared_ptr<const FRenderBatchStructure> Structure;
		std::uint32_t Capacity{};
	};

	std::unordered_map<FIncrementalSourceKey, FSource, FIncrementalSourceKeyHash> Sources;
	std::vector<FBlock> Blocks;
	std::vector<FGroup> Groups;
	std::uint64_t Scene{};
	std::uint64_t ResourceRevision{};
	std::uint64_t Access{};
	ERHIDepthFormat Depth{};
	std::uint32_t ColorCount{};
	FGraphicsTarget Target;
	std::weak_ptr<const void> LocalContents;
	std::shared_ptr<const void> Structure;
	bool bStructureChanged{};

	static FIncrementalSourceKey Key(const FRenderItem& InItem);
	bool UpdateMembership(const FRenderSceneSnapshot& InSnapshot, std::uint64_t InAccess,
	                      std::vector<std::size_t>& OutAdded, FRenderBatchStats& OutStats);
	void RefreshIndices();
	bool RefreshGroups(const FRenderSceneSnapshot& InSnapshot, std::vector<FRenderBatchSignature>& OutSignatures);
	bool Insert(const FRenderSceneSnapshot& InSnapshot, std::size_t InIndex, const FRenderBatchSignature& InSignature,
	            std::uint32_t InCapacity, std::size_t InBlockLimit, std::vector<FRenderBatchSignature>& InSignatures);
	void Remove(std::unordered_map<FIncrementalSourceKey, FSource, FIncrementalSourceKeyHash>::iterator InSource);
};
} // namespace Hyperion
