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
	std::size_t PrimitiveCount() const;
	std::shared_ptr<const FRenderResource> GetResource() const;
	void Remove();

private:
	FTaskHandle Publish();
	FRenderSceneClient& Scene;
	std::shared_ptr<const FModelAsset> Asset;
	std::shared_ptr<const FSceneModelData> Data;
	std::shared_ptr<const FRenderResource> Resource;
	std::vector<FModelInstance> Instances;
	std::vector<FRenderBinding> Bindings;
	FMat4 World = Identity();
	FMaterialOverride Material;
	std::uint64_t Revision = 1;
	bool bVisible = true;
};
} // namespace Hyperion
