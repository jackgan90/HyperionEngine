#pragma once
#include "Hyperion/Renderer/SceneBridge.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"

namespace Hyperion
{
struct FSceneViewerPlugin::FImpl
{
	FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets, std::filesystem::path InPath)
	    : Session(InSession), Tasks(InTasks), Assets(InAssets), Path(std::move(InPath))
	{
	}

	struct FLoad
	{
		TAssetRequest<FModelAsset> Request;
		TAsyncResult<FSceneModelData> Preparation;
		std::string Error;
		bool bComplete{};
	};

	struct FInstance
	{
		FSceneHandle Handle;
		std::string Asset;
	};

	FRenderSession& Session;
	FTaskSystem& Tasks;
	FAssetService& Assets;
	std::filesystem::path Path;
	FScene Scene;
	std::unique_ptr<FSceneRenderBridge> Bridge;
	TAssetRequest<FSceneManifest> ManifestRequest;
	std::shared_ptr<const FSceneManifest> Manifest;
	std::map<std::string, FLoad> Loads;
	std::vector<FInstance> Instances;
	std::string Status = "Loading scene...";
	std::string Error;
	std::size_t Selected{};
	bool bStopped{};
	bool bReady{};
	std::pair<std::uint64_t, std::uint64_t> StatusRevision;
	bool bDragging{};
	bool bFrozen{};
	bool bBounds{};
	bool bAnimate{};
	bool bInstanceBatching = true;
	FVec2 LastMouse;
	FVec3 Target;
	float Distance = 12;
	float Radius = 3;
	float Yaw{};
	float Pitch = .25f;
	float AnimationTime{};
	FRenderView LastView;
	FMat4 FrozenView = Identity();
	ESceneCullingMode Mode = ESceneCullingMode::Bvh;
	void BeginManifest();
	void PollModels();
	void UpdateCamera(FRenderFrame& InFrame);
	void UpdateStatus();
	void DrawBounds(FGui& InGui) const;
};
} // namespace Hyperion
