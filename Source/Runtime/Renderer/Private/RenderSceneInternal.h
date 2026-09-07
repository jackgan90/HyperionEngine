#pragma once
#include "Hyperion/Renderer/RenderScene.h"
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
	void Publish(ERenderPrimitiveStatus InState, std::uint64_t InRevision, std::string InError = {},
	             std::shared_ptr<const FRenderResource> InResource = {}, std::uint32_t InSection = 0);
};

class FRenderScene
{
public:
	explicit FRenderScene(FTaskSystem& InTasks) : Tasks(InTasks)
	{
		Tasks.Require({EDomain::Render});
	}

	~FRenderScene();
	void Create(FRenderPrimitiveHandle InHandle, FRenderPrimitiveState InState, std::uint64_t InGroup,
	            const FRenderPrimitiveFactory& InFactory, std::shared_ptr<FRenderBindingResult> InResult);
	void Update(std::vector<FRenderPrimitiveUpdate> InUpdates);
	void Remove(FRenderPrimitiveHandle InHandle);
	FRenderSceneSnapshot Collect(FRenderView InView);

private:
	struct FEntry
	{
		FRenderPrimitiveHandle Handle;
		std::unique_ptr<IRenderPrimitive> Primitive;
		std::shared_ptr<FRenderBindingResult> Result;
		std::uint64_t Group{};
		FBounds Bounds;
	};

	FTaskSystem& Tasks;
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
