#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Renderer/RenderPlugin.h"

namespace Hyperion
{
class FRenderSession;

class FModelViewerPlugin final : public IScenePlugin
{
public:
	FModelViewerPlugin(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
	                   std::filesystem::path InPath);
	~FModelViewerPlugin() override;
	void Start() override;
	void Update(FRenderFrame& InFrame) override;
	void Stop() noexcept override;
	void Input(std::span<const FInputEvent> InEvents, bool bInMouseCaptured, bool bInKeyboardCaptured);
	const std::string& Status() const;
	const std::string& Error() const;
	bool Ready() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

void RegisterModelViewerPlugin(FPluginRegistry& InRegistry, FRenderSession& InSession, FTaskSystem& InTasks,
                               FAssetService& InAssets, const std::filesystem::path& InPath);
} // namespace Hyperion
