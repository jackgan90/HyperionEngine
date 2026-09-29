#pragma once
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Scene/ObjectPlacement.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <map>

namespace Hyperion
{
struct FPlacementCandidate
{
	std::string Id;
	std::string Label;
	FSceneNode Node;
	std::optional<FAssetRef> Model;
	std::string Icon;
	bool bPreferModelMaterials{};
};

struct FPlacementModel
{
	std::string Asset;
	std::shared_ptr<const FSceneModelData> Data;
	std::shared_ptr<const FRenderResource> Resource;
	std::string Error;
};

// Main-owned preparation and document mutations shared by GUI and automation.
class FPlacementService
{
public:
	static FPlacementCandidate Candidate(const FPlaceableObject& InObject);
	static FPlacementCandidate ModelCandidate(FAssetRef InReference, FAssetService& InAssets);
	void Prepare(const FPlacementCandidate& InCandidate, FSceneInstance& InScene);
	void Poll(FSceneInstance& InScene, FRenderSession& InSession);
	std::string Unavailable(const FPlacementCandidate& InCandidate) const;
	FSceneHandle Commit(const FPlacementCandidate& InCandidate, FVec3 InPosition, FSceneEditDocument& InDocument);
	std::map<std::string, FPlacementModel> Models;
};
} // namespace Hyperion
