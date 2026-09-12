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
	FSceneHandle Selected;
	std::string EditError;
	FSceneHandle PropertySelection;
	std::optional<FSceneHandle> ProposedParent;
	bool bKeepWorld = true;
	void DrawSceneSettings(FGui& InGui);
	bool bStopped{};
	bool bDragging{};
	bool bFrozen{};
	bool bBounds{};
	bool bAnimate{};
	bool bInstanceBatching = true;
	FVec2 LastMouse;
	float AnimationTime{};
	FRenderView LastView;
	FMat4 FrozenView = Identity();
	ESceneCullingMode Mode = ESceneCullingMode::Bvh;
	void BeginManifest();
	void DrawNodes(FGui& InGui);
	void DrawNodeProperties(FGui& InGui);
	void UpdateCamera(FRenderFrame& InFrame);
	void DrawBounds(FGui& InGui) const;
};
} // namespace Hyperion
