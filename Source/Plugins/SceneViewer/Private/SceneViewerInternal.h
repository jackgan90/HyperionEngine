#pragma once
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"

namespace Hyperion
{
struct FSceneViewerPlugin::FImpl
{
	FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets, std::filesystem::path InPath)
	    : Tasks(InTasks), Assets(InAssets), Path(std::move(InPath)), Scene(InSession, InTasks, InAssets)
	{
	}

	FTaskSystem& Tasks;
	FAssetService& Assets;
	FSceneViewerPlugin* Owner{};
	std::optional<TAsyncResult<bool>> Save;
	std::filesystem::path SavePath;
	std::string SaveStatus;
	void PollSave();
	void DrawSave(FGui& InGui);
	std::filesystem::path Path;
	FSceneInstance Scene;
	std::shared_ptr<const FSceneManifest> Manifest;
	std::string Status = "Loading scene...";
	std::string Error;
	std::size_t Selected{};
	bool bStopped{};
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
	void UpdateCamera(FRenderFrame& InFrame);
	void DrawBounds(FGui& InGui) const;
};
} // namespace Hyperion
