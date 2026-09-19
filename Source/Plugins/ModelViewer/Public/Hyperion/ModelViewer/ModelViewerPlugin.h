#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Renderer/RenderPlugin.h"

namespace Hyperion
{
class FRenderSession;
class FSceneInstance;

class FModelViewerPlugin final : public IScenePlugin
{
public:
	FModelViewerPlugin(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
	                   std::filesystem::path InPath);
	~FModelViewerPlugin() override;
	void Start() override;

	bool HasScene() const override
	{
		return true;
	}

	FSceneInstance& GetSceneInstance() override;
	void Update(FRenderFrame& InFrame) override;
	void Stop() noexcept override;
	void Input(std::span<const FInputEvent> InEvents, bool bInMouseCaptured, bool bInKeyboardCaptured) override;
	const std::string& Status() const override;
	const std::string& Error() const override;
	bool Ready() const override;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

void RegisterModelViewerPlugin(FPluginRegistry& InRegistry, FRenderSession& InSession, FTaskSystem& InTasks,
                               FAssetService& InAssets, const std::filesystem::path& InPath);
void RegisterModelViewerPlugin(FPluginRegistry& InRegistry, const std::filesystem::path& InPath);
} // namespace Hyperion
