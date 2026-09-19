#pragma once
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/GuiRenderer/Diagnostics.h"
#include "Hyperion/Renderer/FramePipeline.h"
#include "Hyperion/Renderer/RenderPlugin.h"
#include <array>
#include <optional>

namespace Hyperion
{
class FGuiRenderer;
FDebugActions DrawFrameCaptureControls(FGui& InGui, FAppSettings& InSettings, const FFrameCaptureMetrics& InMetrics);
FDebugActions DrawProfilingControls(FGui& InGui, const FProfileStatus& InStatus);

FDebugActions DrawDebugPanel(FGui& InGui, FAppSettings& InSettings, const FDebugMetrics& InMetrics,
                             FSize InLogicalSize);

class FDebugUiPlugin final : public IRenderPlugin
{
public:
	FDebugUiPlugin(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont);
	explicit FDebugUiPlugin(FGuiRenderer& InRenderer);
	~FDebugUiPlugin() override;
	void Start() override;
	void Start(FPluginContext& InContext) override;
	void Stop() noexcept override;
	void Prepare(const FGuiDrawData& InData);
	void Build(FRenderGraph& InGraph, const FRenderFrame& InFrame) override;
	void BuildDeferred(FRenderGraph& InGraph, FGuiDrawData InData);

private:
	struct FImpl;
	std::shared_ptr<FImpl> Impl;
};

void RegisterDebugUiPlugin(FPluginRegistry& InRegistry, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                           FTaskSystem& InTasks, const FImage& InFont);
void RegisterDebugUiPlugin(FPluginRegistry& InRegistry);

} // namespace Hyperion
