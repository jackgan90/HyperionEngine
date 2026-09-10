#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Renderer/SceneInstance.h"

namespace Hyperion
{
struct FSceneInstance::FImpl
{
	FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets);

	struct FLoad
	{
		TAssetRequest<FModelAsset> Request;
		TAsyncResult<FSceneModelData> Preparation;
		std::shared_ptr<const FSceneModelData> Data;
		std::string Error;
		bool bComplete{};
	};

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
	void UpdateStatus();
	void RequireOpen() const;
};
} // namespace Hyperion
