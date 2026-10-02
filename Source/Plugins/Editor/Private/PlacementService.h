#pragma once
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Scene/ObjectPlacement.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <map>

namespace Hyperion
{
enum class EPlacementPreparationState
{
	Ready,
	Pending,
	Failed
};

enum class EPlacementPreparationStage
{
	None,
	Scene,
	ModelLoading,
	ModelUpload,
	PreviewMaterial,
	IconLoading,
	IconUpload
};

struct FPlacementPreparation
{
	EPlacementPreparationState State = EPlacementPreparationState::Ready;
	EPlacementPreparationStage Stage = EPlacementPreparationStage::None;
	std::string Error;
};

struct FPlacementPreparationContext
{
	bool bSceneAvailable{};
	FPlacementPreparation PreviewMaterial;
	std::optional<FPlacementPreparation> Icon;
};

// Presentation only; callers must inspect State for admission and completion.
std::string FormatPlacementPreparation(const FPlacementPreparation& InPreparation);

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
	bool bLoadComplete{};
	std::optional<std::string> UploadError;
};

// Main-owned preparation and document mutations shared by GUI and automation.
class FPlacementService
{
public:
	static FPlacementCandidate Candidate(const FPlaceableObject& InObject);
	static FPlacementCandidate ModelCandidate(FAssetRef InReference, FAssetService& InAssets);
	void Prepare(const FPlacementCandidate& InCandidate, FSceneInstance& InScene);
	void Poll(FSceneInstance& InScene, FRenderSession& InSession);
	static FPlacementPreparation ResolvePreparation(const FPlacementCandidate& InCandidate,
	                                                const FPlacementPreparation& InModel,
	                                                const FPlacementPreparationContext& InContext);
	FPlacementPreparation GetPreparation(const FPlacementCandidate& InCandidate,
	                                     const FPlacementPreparationContext& InContext) const;
	FSceneHandle Commit(const FPlacementCandidate& InCandidate, FVec3 InPosition, FSceneEditDocument& InDocument,
	                    const FPlacementPreparationContext& InContext);
	std::map<std::string, FPlacementModel> Models;

private:
	FPlacementPreparation ModelPreparation(const FPlacementCandidate& InCandidate) const;
};
} // namespace Hyperion
