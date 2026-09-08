#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderPlugin.h"

namespace Hyperion
{
class FRenderSession;

class FSceneViewerPlugin final : public IScenePlugin
{
public:
	FSceneViewerPlugin(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
	                   std::filesystem::path InPath);
	~FSceneViewerPlugin() override;
	void Start() override;
	void Update(FRenderFrame& InFrame) override;
	void Stop() noexcept override;
	void Input(std::span<const FInputEvent> InEvents, bool bInMouseCaptured, bool bInKeyboardCaptured);
	void DrawGui(FGui& InGui, const FSceneVisibilityStats& InStats, bool bInForceOrdinary = false);
	bool Ready() const;
	const std::string& Status() const;
	const std::string& Error() const;
	std::size_t ModelCount() const;
	void SetCullingMode(ESceneCullingMode InMode);
	void SetFrozen(bool bInFrozen);
	void Fit();
	void DuplicateSelected();
	void AddModel();
	void RemoveSelected();
	void ToggleSelected();
	void MoveSelected(float InOffset);

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

void RegisterSceneManifestLoader(FAssetService& InAssets);
void RegisterSceneViewerPlugin(FPluginRegistry& InRegistry, FRenderSession& InSession, FTaskSystem& InTasks,
                               FAssetService& InAssets, const std::filesystem::path& InPath);
} // namespace Hyperion
