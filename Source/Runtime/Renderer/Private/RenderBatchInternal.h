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

template<typename TValue>
bool SameBatchOwner(const std::weak_ptr<const TValue>& InWeak, const std::shared_ptr<const TValue>& InStrong)
{
	// An expired nonempty overlay is different from no overlay, even though both lock/get as null.
	return !InWeak.owner_before(InStrong) && !InStrong.owner_before(InWeak) && InWeak.lock() == InStrong;
}

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

	struct FInstanceContract
	{
		std::weak_ptr<const FCompiledMaterialDefinition> Program;
		std::vector<std::size_t> Parameters;
	};

	struct FInstanceValueReference
	{
		explicit FInstanceValueReference(const std::shared_ptr<const FMaterialValue>& InValue)
		    : Owner(InValue), Address(InValue.get())
		{
		}

		bool Matches(const std::shared_ptr<const FMaterialValue>& InValue) const;

	private:
		std::weak_ptr<const FMaterialValue> Owner;
		const FMaterialValue* Address{};
	};

	struct FPreparedItem
	{
		FRenderPrimitiveHandle Primitive;
		std::uint64_t LocalId{};
		std::weak_ptr<const void> Lifetime;
		// Only this validation cursor changes during fresh planning. Instance value proofs and source identity stay
		// fixed.
		std::weak_ptr<const FRenderResource> Resource;
		std::weak_ptr<const FRenderMaterial> Surface;
		std::weak_ptr<const FResolvedMaterialParameters> Values;
		FMaterialValueTable::FLocalIdentity LocalValues;
		std::weak_ptr<const void> Resources;
		std::shared_ptr<const FInstanceContract> Contract;
		std::vector<FInstanceValueReference> InstanceValues;
		std::uint32_t Section{};
		bool bMirrored{};
		std::optional<FMaterialDynamicState> Dynamic;
		std::weak_ptr<const FMaterialSharedParameters> Shared;
		std::size_t MetadataBytes{};
		bool MatchesSource(const FRenderItem& InItem) const;
		bool MatchesState(const FRenderItem& InItem) const;
		bool MatchesValues(const FRenderItem& InItem) const;
		bool MatchesInstances(const FRenderItem& InItem) const;
		void RefreshValues(const FRenderItem& InItem);
	};

	struct FPreparedEntry
	{
		std::shared_ptr<FPreparedItem> Input;
		std::list<FBatchItemKey>::iterator Recent;
	};

	struct FPreparedReference
	{
		FPreparedReference(const std::shared_ptr<const FPreparedItem>& InInput) : Owner(InInput), Address(InInput.get())
		{
		}

		const FPreparedItem* Get() const
		{
			// All strong preparation ownership and cache mutation belong to Render. During a cache read,
			// an unexpired weak owner proves this address remains live without an extra strong-reference RMW.
			return Owner.expired() ? nullptr : Address;
		}

	private:
		std::weak_ptr<const FPreparedItem> Owner;
		const FPreparedItem* Address{};
	};

	struct FChunkEntry
	{
		std::vector<FPreparedReference> Members;
		std::shared_ptr<const FInstanceBatchData> Data;
		std::size_t Bytes{};
		std::uint64_t Access{};
		std::list<FBatchItemKey>::iterator Recent;
	};

	struct FPlanItem
	{
		FPreparedReference Input;
		std::weak_ptr<const FResolvedMaterialParameters> Values;
		std::weak_ptr<const FMaterialSharedParameters> Shared;
		bool Matches(const FRenderItem& InItem, bool bInSharedRefresh) const;
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
	std::map<std::pair<const FCompiledMaterialDefinition*, std::string>, std::shared_ptr<const FInstanceContract>>
	    Contracts;
	std::map<FBatchItemKey, FPreparedEntry> Prepared;
	std::list<FBatchItemKey> RecentPrepared;
	std::vector<std::shared_ptr<const FPreparedItem>> CurrentInputs;
	std::size_t PreparedMetadataBytes{};
	std::optional<FBatchItemKey> PreparedCursor;
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
	std::shared_ptr<const FInstanceContract> Contract(const FRenderSceneSnapshot& InSnapshot, const FRenderItem& InItem,
	                                                  FRenderBatchStats& OutStats);
	void PrepareInputs(const FRenderSceneSnapshot& InSnapshot, FRenderBatchStats& OutStats);
	std::shared_ptr<const FPreparedItem> PrepareInput(const FRenderSceneSnapshot& InSnapshot, const FRenderItem& InItem,
	                                                  FRenderBatchStats& OutStats);
	void ErasePrepared(std::map<FBatchItemKey, FPreparedEntry>::iterator InEntry);
	void ReserveChunkBytes(std::size_t InBytes, FRenderBatchStats& OutStats);
	void CollectPrepared();
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
