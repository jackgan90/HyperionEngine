#pragma once
#include "hyperion/Gui.h"
#include "hyperion/RenderGraph.h"

namespace Hyperion
{
struct FDebugMetrics
{
	FDeviceStats Device;
	std::vector<FExecutionStats> Threads;
	std::vector<float> FrameMilliseconds;
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
	FDebugUiPlugin(FRhiDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont);
	~FDebugUiPlugin() override;
	void Start() override;
	void Stop() noexcept override;
	void Prepare(const FGuiDrawData& InData);
	void Build(FRenderGraph& InGraph, const FRenderFrame& InFrame) override;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

void RegisterDebugUiPlugin(FPluginRegistry& InRegistry, FRhiDevice& InDevice, FShaderCompiler& InCompiler,
                           FTaskSystem& InTasks, const FImage& InFont);
} // namespace Hyperion
