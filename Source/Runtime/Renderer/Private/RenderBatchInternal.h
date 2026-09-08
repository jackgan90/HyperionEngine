#pragma once
#include "Hyperion/Renderer/RenderBatch.h"
#include <list>
#include <map>
#include <tuple>

namespace Hyperion
{
using FBatchItemKey =
    std::tuple<std::uint64_t, std::string, std::uint64_t, std::uint32_t, std::uint64_t, std::uint64_t>;

FBatchItemKey BatchItemKey(const FRenderSceneSnapshot& InSnapshot, const FRenderItem& InItem);

struct FRenderBatchSystem::FImpl
{
	struct FItemEntry
	{
		std::weak_ptr<const FResolvedMaterialParameters> Values;
		std::weak_ptr<const void> Resources;
		std::weak_ptr<const void> Lifetime;
		std::shared_ptr<const FRenderBatchCandidate> Candidate;
		std::uint32_t Section{};
		bool bMirrored{};
		std::optional<FMaterialDynamicState> Dynamic;
		std::list<FBatchItemKey>::iterator Recent;
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
		std::uint32_t Section{};
		bool bMirrored{};
		std::optional<FMaterialDynamicState> Dynamic;
		bool Matches(const FRenderItem& InItem) const;
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
	std::vector<std::unique_ptr<IRenderBatchStrategy>> Strategies;
	std::map<FBatchItemKey, FItemEntry> Items;
	std::list<FBatchItemKey> RecentItems;
	std::map<FBatchItemKey, FChunkEntry> Chunks;
	std::list<FBatchItemKey> RecentChunks;
	std::size_t ChunkBytes{};
	std::uint64_t Access{};
	bool bStarted{};
	std::map<std::pair<std::uint64_t, std::string>, FPlanEntry> Plans;
	std::size_t PlanItems{};

	FImpl(FTaskSystem& InTasks, FRHICapabilities InCapabilities, FRenderBatchLimits InLimits)
	    : Tasks(InTasks), Capabilities(std::move(InCapabilities)), Limits(InLimits)
	{
		Strategies.push_back(std::make_unique<FInstanceBatchStrategy>());
	}

	std::shared_ptr<const FRenderBatchCandidate> Describe(const FRenderSceneSnapshot& InSnapshot,
	                                                      const FRenderItem& InItem, FGraphicsTarget InTarget,
	                                                      FRenderBatchStats& OutStats);
	std::shared_ptr<const FInstanceBatchData> Data(const FRenderSceneSnapshot& InSnapshot, const FRenderBatch& InBatch,
	                                               FRenderBatchStats& OutStats);
	std::shared_ptr<FRenderBatchPlan> BuildFresh(const FRenderSceneSnapshot& InSnapshot, bool bInEnabled);
	std::shared_ptr<FRenderBatchPlan> ReusePlan(const FRenderSceneSnapshot& InSnapshot);
	void CachePlan(const FRenderSceneSnapshot& InSnapshot, const FRenderBatchPlan& InPlan);
	void Retire(const FRenderSceneSnapshot& InSnapshot);
	void EraseChunk(std::map<FBatchItemKey, FChunkEntry>::iterator InEntry);
};
} // namespace Hyperion
