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
	FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets, bool bInPrepareQueries);
	bool bPrepareQueries{};

	struct FLoad
	{
		std::uint64_t Epoch{};
		FCancellationToken Cancellation;
		TAsyncResult<FSceneModelData> Preparation;
		std::shared_ptr<const FSceneModelData> Data;
		std::string Error;
		bool bComplete{};
	};

	struct FSelectedMaterials
	{
		FSceneHandle Handle;
		std::uint64_t Epoch{};
		FSceneMaterialSelection Surface;
		std::map<std::uint32_t, FSceneMaterialSelection> Sections;
		std::string Error;
	};

	struct FPendingMaterial
	{
		bool bHasStoredSelection{};
		bool bSurfaceEdited{};
		std::set<std::uint32_t> EditedSections;
		FSceneHandle SelectionSource;
	};

	std::map<FSceneHandle, FPendingMaterial> PendingMaterials;
	FPendingMaterial PrepareMaterialEdits(const FPendingMaterial& InPending, const FSceneModelComponent& InBefore,
	                                      const FSceneModelComponent& InAfter) const;
	void ApplyLoadedMaterials(FSceneModelComponent& InModel, const FSelectedMaterials& InSelection,
	                          const FPendingMaterial& InPending) const;
	TAsyncResult<std::vector<FSelectedMaterials>> MaterialPreparation;
	std::map<FSceneHandle, FSelectedMaterials> SelectedMaterials;
	FCancellationToken MaterialCancellation;
	bool bMaterialsComplete = true;

	struct FSkyLoad
	{
		FSceneHandle Handle;
		FAssetRef Reference;
		FCancellationToken Cancellation;
		TAsyncResult<FSceneSkyData> Preparation;
		TAsyncResult<bool> Upload;
		std::shared_ptr<const FSceneSkyData> Data;
		std::string Error;
		bool bGpuSubmitted{};
		bool bComplete{};
	};

	std::map<FSceneHandle, std::shared_ptr<FSkyLoad>> SkyLoads;
	std::vector<std::shared_ptr<FSkyLoad>> RetiredSkyLoads;
	void PollSkies();
	void PollSky(FSkyLoad& InLoad);
	void CloseSkies();

	FRenderSession& Session;
	FTaskSystem& Tasks;
	FAssetService& Assets;
	FScene Scene;
	std::uint64_t LoadEpoch{1};
	FSceneHandle AddNode(FSceneNode InNode, bool bInResolveData);
	void RefreshModels();
	void ForgetRemovedModels();
	std::unique_ptr<FSceneRenderBridge> Bridge;
	std::filesystem::path Path;
	TAssetRequest<FSceneManifest> ManifestRequest;
	std::shared_ptr<const FSceneManifest> Manifest;
	std::map<std::string, FLoad> Loads;
	std::vector<FSceneInstanceModel> Models;
	FSceneInstanceStatus Status;
	std::pair<std::uint64_t, std::uint64_t> ModelStatusRevision;
	bool bModelStatusDirty = true;
	void BeginManifest();
	void PollModels();
	void BeginMaterials();
	void PollMaterials();
	void PublishModels();
	void UpdateStatus();
	void RefreshModelStatus();
	void RequireOpen() const;
};
} // namespace Hyperion
