#pragma once
#include "Hyperion/Renderer/RenderResources.h"
#include "Hyperion/Renderer/RenderScene.h"

namespace Hyperion
{
// Main logical group, separate from the immutable Scene asset and Render-owned primitives.
class FModel
{
public:
	FModel(FRenderSceneClient& InScene, FRenderResourceService& InResources,
	       std::shared_ptr<const FModelAsset> InAsset);
	FModel(FRenderSceneClient& InScene, FRenderResourceService& InResources, const FSceneModel& InModel);
	~FModel();
	FModel(const FModel&) = delete;
	FModel& operator=(const FModel&) = delete;
	FTaskHandle SetTransform(FMat4 InWorld);
	FTaskHandle SetVisible(bool bInVisible);
	FTaskHandle SetMaterial(FMaterialOverride InMaterial);
	FTaskHandle SetState(const FSceneModel& InModel);
	bool IsReady() const;
	std::string GetError() const;
	std::vector<FRenderDrawResult> GetDrawResults() const;
	std::size_t PrimitiveCount() const;
	std::shared_ptr<const FRenderResource> GetResource() const;
	void Remove();

private:
	FModel(FRenderSceneClient& InScene, FRenderResourceService& InResources, const FSceneModel& InModel,
	       bool bInDeferred);
	using FFrozenMaterials = std::map<const FMaterialInstance*, std::shared_ptr<const FMaterialSnapshot>>;

	struct FPreparedUpdate
	{
		FSceneModel State;
		std::uint64_t Revision{};
		std::vector<FRenderPrimitiveUpdate> Updates;
	};

	FPreparedUpdate PrepareState(const FSceneModel& InModel, FFrozenMaterials& InFrozen) const;
	void CommitState(FPreparedUpdate InUpdate) noexcept;
	static std::shared_ptr<const FMaterialSnapshot> FreezeSelection(const FSceneMaterialSelection& InSelection,
	                                                                FFrozenMaterials& InFrozen);
	FRenderSceneClient& Scene;
	FRenderResourceService& Resources;
	std::shared_ptr<const FModelAsset> Asset;
	std::shared_ptr<const FSceneModelData> Data;
	std::shared_ptr<const FRenderResource> Resource;
	std::vector<FModelInstance> Instances;
	std::vector<FRenderBinding> Bindings;
	FSceneModel Current;
	std::uint64_t Revision{};
	friend class FSceneRenderBridge;
};
} // namespace Hyperion
