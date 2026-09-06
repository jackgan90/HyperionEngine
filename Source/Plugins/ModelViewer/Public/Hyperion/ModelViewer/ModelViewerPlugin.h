#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Renderer/RenderPlugin.h"

namespace Hyperion
{
class FModelViewerPlugin final : public IRenderPlugin
{
public:
	FModelViewerPlugin(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FAssetService& InAssets,
	                   std::filesystem::path InPath);
	~FModelViewerPlugin() override;
	void Start() override;
	void Build(FRenderGraph& InGraph, const FRenderFrame& InFrame) override;
	void Stop() noexcept override;
	void Input(std::span<const FInputEvent> InEvents, bool bInMouseCaptured, bool bInKeyboardCaptured);
	const std::string& Status() const;
	const std::string& Error() const;
	bool Ready() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

void RegisterModelViewerPlugin(FPluginRegistry& InRegistry, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                               FTaskSystem& InTasks, FAssetService& InAssets, const std::filesystem::path& InPath);
} // namespace Hyperion
