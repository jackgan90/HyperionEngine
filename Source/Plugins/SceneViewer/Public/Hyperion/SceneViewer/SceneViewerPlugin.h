#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Renderer/RenderPlugin.h"

namespace Hyperion
{
class FRenderSession;
class FSceneInstance;

class FSceneViewerPlugin final : public ISceneEditor
{
public:
	FSceneViewerPlugin(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
	                   std::filesystem::path InPath);
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
	void SetRenderedView(const std::optional<FRenderView>& InView) override;
	void DuplicateSelected();
	void AddModel();
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
void RegisterSceneViewerPlugin(FPluginRegistry& InRegistry, const std::filesystem::path& InPath);
} // namespace Hyperion
