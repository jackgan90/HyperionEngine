#pragma once
#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneEditTarget.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"

namespace Hyperion
{
struct FSceneViewerPlugin::FImpl
{
	FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets, std::filesystem::path InPath)
	    : Tasks(InTasks), Assets(InAssets), Path(std::move(InPath)), Scene(InSession, InTasks, InAssets),
	      SceneTarget(Scene, InAssets)
	{
		Document.Attach(SceneTarget, false);
		Document.SetPath(PathToUtf8(Path));
	}

	FTaskSystem& Tasks;
	FAssetService& Assets;
	FSceneViewerPlugin* Owner{};
	std::string SaveStatus;
	void PollSave();
	void DrawSave(FGui& InGui);
	std::filesystem::path Path;
	FSceneInstance Scene;
	FSceneInstanceEditTarget SceneTarget;
	FSceneEditDocument Document;
	std::shared_ptr<const FSceneManifest> Manifest;
	std::string Status = "Loading scene...";
	std::string Error;
	FSceneHandle Selected;
	std::string EditError;
	FSceneHandle PropertySelection;
	std::optional<FSceneHandle> ProposedParent;
	bool bKeepWorld = true;
	void DrawSceneSettings(FGui& InGui);
	void DrawSkyControls(FGui& InGui);
	std::string SkyPath;
	std::vector<std::string> SkyChoices;
	bool bSkyChoicesInitialized{};
	bool bStopped{};
	FSceneCameraController CameraController;
	FSceneCameraView ViewCamera;
	bool bViewInitialized{};
	std::shared_ptr<const FSceneManifest> ViewManifest;
	bool bFrozen{};
	bool bBounds{};
	bool bLightBounds{};
	void DrawLightProperties(FGui& InGui, const FSceneNode& InNode);
	void DrawLightBounds(FGui& InGui) const;
	bool bAnimate{};
	bool bGuiInteraction{};
	void AdvanceAnimation(float InDeltaSeconds);
	bool bInstanceBatching = true;
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
