#pragma once
#include "Hyperion/Renderer/RenderScene.h"
#include "MaterialEvaluationCache.h"
#include <map>
#include <mutex>
#include <set>

namespace Hyperion
{
struct FRenderBindingResult
{
	std::mutex Mutex;
	FRenderBindingStatus Status;
	std::weak_ptr<const FRenderResource> Resource;
	std::uint32_t Section{};
	std::shared_ptr<const FRenderPrimitiveState> Admitted;
	FRenderDrawResult LastDraw;
	std::shared_ptr<const std::atomic_uint64_t> LastDrawFrame;
	std::shared_ptr<const FMaterialParameterSchema> ValidatedSchema;
	void Publish(ERenderPrimitiveStatus InState, std::uint64_t InRevision, std::string InError = {},
	             std::shared_ptr<const FRenderResource> InResource = {}, std::uint32_t InSection = 0,
	             std::shared_ptr<const FRenderPrimitiveState> InAdmitted = {});
};

class FRenderScene
{
public:
	explicit FRenderScene(FTaskSystem& InTasks, std::uint64_t InIdentity,
	                      std::function<std::shared_ptr<const void>()> InScopeFactory,
	                      std::function<void()> InOnChanged)
	    : Tasks(InTasks), Identity(InIdentity), ScopeFactory(std::move(InScopeFactory)),
	      OnChanged(std::move(InOnChanged))
	{
		Tasks.Require({EDomain::Render});
	}

	~FRenderScene();
	void Create(FRenderPrimitiveHandle InHandle, FRenderPrimitiveState InState, std::uint64_t InGroup,
	            const FRenderPrimitiveFactory& InFactory, std::shared_ptr<FRenderBindingResult> InResult);
	void Update(std::vector<FRenderPrimitiveUpdate> InUpdates);
	void Remove(FRenderPrimitiveHandle InHandle);
	FRenderSceneSnapshot Collect(FRenderView InView, bool bInRefresh = true, std::uint64_t InResourceRevision = 0,
	                             FRenderSceneSnapshot* InPrevious = nullptr);
	FSceneVisibilityStats BeginViews();
	std::optional<std::uint64_t> GetCollectionRevision() const;
	std::vector<FBounds> QueryBounds(const ISceneVisibility& InVisibility) const;

private:
	struct FEntry
	{
		FRenderPrimitiveHandle Handle;
		std::unique_ptr<IRenderPrimitive> Primitive;
		std::shared_ptr<FRenderBindingResult> Result;
		std::uint64_t Group{};
		FBounds Bounds;
		std::shared_ptr<const void> Lifetime;
		std::shared_ptr<FMaterialEvaluationCache> EvaluationCache = std::make_shared<FMaterialEvaluationCache>();
		std::optional<std::vector<FRenderItem>> Collection;
		std::uint64_t ResourceRevision{};
		std::vector<FRenderItem> Collect(const FRenderView& InView, std::uint64_t InResourceRevision);
	};

	FTaskSystem& Tasks;
	std::uint64_t Identity{};
	std::function<std::shared_ptr<const void>()> ScopeFactory;
	std::function<void()> OnChanged;
	std::uint64_t Revision = 1;
	mutable std::optional<bool> Cacheable;
	void Invalidate();
	std::map<std::uint32_t, FEntry> Entries;
	std::map<std::uint64_t, std::set<std::uint32_t>> Groups;
	std::set<std::uint64_t> DirtyGroups;
	std::set<std::uint64_t> UnboundedGroups;
	std::unique_ptr<ISceneSpatialIndex> Spatial = CreateBvhSpatialIndex();
	void RefreshSpatial(FSceneVisibilityStats& OutStats);
};

struct FRenderSceneMailbox
{
	explicit FRenderSceneMailbox(FTaskSystem& InTasks);
	FTaskSystem& Tasks;
	std::mutex Admission;
	std::uint64_t Identity{};
	std::uint64_t NextGroup{};
	std::uint64_t LogicalScene{};
	bool bClosed{};
	std::vector<std::uint64_t> Generations;
	std::vector<bool> Active;
	// Only Render touches the scene, including destruction; clients retain this empty shell after Close.
	std::unique_ptr<FRenderScene> Scene;
	FTaskHandle Last;
	FTaskHandle Remove(FRenderPrimitiveHandle InHandle, const std::shared_ptr<FRenderBindingResult>& InResult);
};
} // namespace Hyperion
