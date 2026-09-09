#pragma once
#include "Hyperion/Renderer/RenderBatch.h"
#include "InstanceDataCache.h"
#include <list>
#include <map>
#include <tuple>

namespace Hyperion
{
using FBatchItemKey =
    std::tuple<std::uint64_t, std::string, std::uint64_t, std::uint32_t, std::uint64_t, std::uint64_t>;

FBatchItemKey BatchItemKey(const FRenderSceneSnapshot& InSnapshot, const FRenderItem& InItem);

// Temporary per-view sharing. Only parameters covered by one complete shared overlay may use this proof.
struct FSharedBatchValueCache
{
	struct FEntry
	{
		std::shared_ptr<const FMaterialSharedParameters> Update;
		std::shared_ptr<const FRenderBatchValues> Values;
	};

	std::map<std::pair<const FCompiledMaterialPass*, const FMaterialSharedParameters*>, FEntry> Entries;
	std::shared_ptr<const FRenderBatchValues> Get(const FCompiledMaterialPass& InPass, const FRenderItem& InItem);
};

struct FRenderBatchSystem::FImpl
{
	struct FItemEntry
	{
		std::weak_ptr<const FResolvedMaterialParameters> Values;
		std::weak_ptr<const void> Resources;
		std::weak_ptr<const void> Lifetime;
		std::shared_ptr<FRenderBatchCandidate> Candidate;
		std::uint32_t Section{};
		bool bMirrored{};
		std::optional<FMaterialDynamicState> Dynamic;
		std::list<FBatchItemKey>::iterator Recent;
		std::weak_ptr<const FMaterialSharedParameters> Shared;
	};

	struct FChunkEntry
	{
		std::vector<FBatchItemKey> Members;
		std::string Vertex;
		std::string Pixel;
		FResourceBindingLayoutDesc Layout;
		std::vector<std::shared_ptr<const FMaterialValue>> Values;
		std::shared_ptr<const FInstanceBatchData> Data;
		std::size_t Bytes{};
		std::uint64_t Access{};
		std::list<FBatchItemKey>::iterator Recent;
	};

	struct FPlanItem
	{
		FRenderPrimitiveHandle Primitive;
		std::uint64_t LocalId{};
		std::weak_ptr<const void> Lifetime;
		std::weak_ptr<const FRenderResource> Resource;
		std::weak_ptr<const FRenderMaterial> Surface;
		std::weak_ptr<const FResolvedMaterialParameters> Values;
		FMaterialValueTable ParameterValues;
		std::weak_ptr<const void> Resources;
		std::vector<std::size_t> InstanceParameters;
		std::uint32_t Section{};
		bool bMirrored{};
		std::optional<FMaterialDynamicState> Dynamic;
		bool Matches(const FRenderItem& InItem, bool bInSharedRefresh) const;
		std::weak_ptr<const FMaterialSharedParameters> Shared;
	};

	struct FPlanEntry
	{
		std::vector<FPlanItem> Inputs;
		std::vector<std::vector<std::size_t>> Members;
		FRenderBatchStats Statistics;
		ERHIDepthFormat Depth{};
		std::uint64_t Access{};
	};

	FTaskSystem& Tasks;
	FRHICapabilities Capabilities;
	FRenderBatchLimits Limits;
	FInstanceDataCache Packing;
	std::vector<std::unique_ptr<IRenderBatchStrategy>> Strategies;
	std::map<FBatchItemKey, FItemEntry> Items;
	std::multimap<std::size_t, std::weak_ptr<const FRenderBatchStructure>> Structures;
	std::list<FBatchItemKey> RecentItems;
	std::map<FBatchItemKey, FChunkEntry> Chunks;
	std::list<FBatchItemKey> RecentChunks;
	std::size_t ChunkBytes{};
	std::uint64_t Access{};
	bool bStarted{};
	std::map<std::pair<std::uint64_t, std::string>, FPlanEntry> Plans;
	std::size_t PlanItems{};
	std::array<std::uint64_t, 3> RetiredFamily{};

	FImpl(FTaskSystem& InTasks, FRHICapabilities InCapabilities, FRenderBatchLimits InLimits)
	    : Tasks(InTasks), Capabilities(std::move(InCapabilities)), Limits(InLimits),
	      Packing({InLimits.MaxItems, InLimits.MaxChunks, InLimits.MaxBytes / 2})
	{
		Strategies.push_back(std::make_unique<FInstanceBatchStrategy>());
	}

	std::shared_ptr<const FRenderBatchCandidate> Describe(const FRenderSceneSnapshot& InSnapshot,
	                                                      const FRenderItem& InItem, FGraphicsTarget InTarget,
	                                                      FRenderBatchStats& OutStats,
	                                                      FSharedBatchValueCache& InShared);
	void CanonicalizeStructure(FRenderBatchSignature& InSignature);
	std::shared_ptr<const FInstanceBatchData> Data(const FRenderSceneSnapshot& InSnapshot, const FRenderBatch& InBatch,
	                                               FRenderBatchStats& OutStats);
	std::shared_ptr<FRenderBatchPlan> BuildFresh(const FRenderSceneSnapshot& InSnapshot, bool bInEnabled);
	std::shared_ptr<FRenderBatchPlan> ReusePlan(const FRenderSceneSnapshot& InSnapshot);
	void CachePlan(const FRenderSceneSnapshot& InSnapshot, const FRenderBatchPlan& InPlan);
	void Retire(const FRenderSceneSnapshot& InSnapshot);
	void RetireExpired();
	void EraseChunk(std::map<FBatchItemKey, FChunkEntry>::iterator InEntry);
};
} // namespace Hyperion
