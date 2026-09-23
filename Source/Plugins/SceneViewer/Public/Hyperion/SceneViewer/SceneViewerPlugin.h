#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Renderer/RenderPlugin.h"
#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/SceneEditing/ScenePlacement.h"

namespace Hyperion
{
class FRenderSession;
class FSceneInstance;

class FSceneViewerPlugin final : public ISceneEditor, public ISceneViewport, public IScenePlacement
{
public:
	FSceneViewerPlugin(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
	                   std::filesystem::path InPath, bool bInForceOrdinary = false);
	~FSceneViewerPlugin() override;
	void Start() override;
	void Start(FPluginContext& InContext) override;

	bool HasScene() const override
	{
		return true;
	}

	FSceneInstance& GetSceneInstance() override;
	void Update(FRenderFrame& InFrame) override;
	void Stop() noexcept override;
	void Input(std::span<const FInputEvent> InEvents, bool bInMouseCaptured, bool bInKeyboardCaptured) override;
	// Update advances the camera automatically; use this only for manual steps outside that frame update.
	void AdvanceCamera(float InDeltaSeconds);
	void DrawGui(FGui& InGui, const FSceneVisibilityStats& InStats, bool bInForceOrdinary = false,
	             const FLocalLightStatistics& InLights = {});
	bool Ready() const override;
	const std::string& Status() const override;
	const std::string& Error() const override;
	std::size_t ModelCount() const;
	void SetCullingMode(ESceneCullingMode InMode) override;
	void SetFrozen(bool bInFrozen);
	void Fit();
	FSceneViewportState ViewportState() const override;
	void SetViewportCamera(const FSceneCameraView& InCamera) override;
	void FrameScene() override;
	void SetViewportOptions(const FSceneViewportOptions& InOptions) override;
	void SetRenderedView(const std::optional<FRenderView>& InView) override;
	void DuplicateSelected();
	void AddModel();
	FPlacementCatalog PlacementCatalog() const override;
	std::optional<FSceneNodeInfo> PlaceObject(const FScenePlacementRequest& InRequest) override;
	void RemoveSelected();
	void ToggleSelected();
	void MoveSelected(float InOffset);
	TAsyncResult<bool> SaveAsync(const std::filesystem::path& InPath) override;
	const std::string& SaveStatus() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

void RegisterSceneViewerPlugin(FPluginRegistry& InRegistry, FRenderSession& InSession, FTaskSystem& InTasks,
                               FAssetService& InAssets, const std::filesystem::path& InPath);
void RegisterSceneViewerPlugin(FPluginRegistry& InRegistry, const std::filesystem::path& InPath,
                               bool bInForceOrdinary = false);
} // namespace Hyperion
