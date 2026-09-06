#pragma once
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderPlugin.h"

namespace Hyperion
{
struct FDebugMetrics
{
	FDeviceStats Device;
	std::vector<FExecutionStats> Threads;
	std::vector<float> FrameMilliseconds;
	std::string AssetStatus;
};

struct FDebugActions
{
	bool Save{};
	bool Capture{};
};

FDebugActions DrawDebugPanel(FGui& InGui, FAppSettings& InSettings, const FDebugMetrics& InMetrics,
                             FSize InLogicalSize);

class FDebugUiPlugin final : public IRenderPlugin
{
public:
	FDebugUiPlugin(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont);
	~FDebugUiPlugin() override;
	void Start() override;
	void Stop() noexcept override;
	void Prepare(const FGuiDrawData& InData);
	void Build(FRenderGraph& InGraph, const FRenderFrame& InFrame) override;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

void RegisterDebugUiPlugin(FPluginRegistry& InRegistry, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                           FTaskSystem& InTasks, const FImage& InFont);
} // namespace Hyperion
