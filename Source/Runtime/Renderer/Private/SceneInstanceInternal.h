#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Renderer/NativeModel.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneMaterialAssets.h"
#include <set>

namespace Hyperion
{
struct FSceneInstance::FImpl
{
	FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets);

	struct FLoad
	{
		FCancellationToken Cancellation;
		TAsyncResult<FSceneModelData> Preparation;
		std::shared_ptr<const FSceneModelData> Data;
		std::string Error;
		bool bComplete{};
	};

	struct FSelectedMaterials
	{
		std::string Id;
		FSceneMaterialSelection Surface;
		std::map<std::uint32_t, FSceneMaterialSelection> Sections;
		std::string Error;
	};

	struct FPendingMaterial
	{
		bool bHasStoredSelection{};
		bool bSurfaceEdited{};
		std::set<std::uint32_t> EditedSections;
	};

	std::map<std::string, FPendingMaterial> PendingMaterials;
	FPendingMaterial PrepareMaterialEdits(const FPendingMaterial& InPending, const FSceneModel& InBefore,
	                                      const FSceneModel& InAfter) const;
	void ApplyLoadedMaterials(FSceneModel& InModel, const FSelectedMaterials& InSelection,
	                          const FPendingMaterial& InPending) const;
	TAsyncResult<std::vector<FSelectedMaterials>> MaterialPreparation;
	std::map<std::string, FSelectedMaterials> SelectedMaterials;
	FCancellationToken MaterialCancellation;
	bool bMaterialsComplete = true;

	FRenderSession& Session;
	FTaskSystem& Tasks;
	FAssetService& Assets;
	FScene Scene;
	std::unique_ptr<FSceneRenderBridge> Bridge;
	std::filesystem::path Path;
	TAssetRequest<FSceneManifest> ManifestRequest;
	std::shared_ptr<const FSceneManifest> Manifest;
	std::map<std::string, FLoad> Loads;
	std::vector<FSceneInstanceModel> Models;
	FSceneInstanceStatus Status;
	std::pair<std::uint64_t, std::uint64_t> StatusRevision;
	bool bStatusDirty = true;
	void BeginManifest();
	void PollModels();
	void BeginMaterials();
	void PollMaterials();
	void PublishModels();
	void UpdateStatus();
	void RequireOpen() const;
};
} // namespace Hyperion
