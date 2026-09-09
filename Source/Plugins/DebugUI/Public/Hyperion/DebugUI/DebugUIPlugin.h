#pragma once
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderPlugin.h"
#include <array>
#include <optional>

namespace Hyperion
{
struct FFrameCaptureMetrics
{
	bool bCompiled = false;
	bool bAvailable = false;
	bool bBusy = false;
	std::string Status = "RenderDoc support is not compiled in";
	std::string LastCapture;
	std::string OpenStatus;
};

struct FDebugMetrics
{
	FDeviceStats Device;
	std::vector<FExecutionStats> Threads;
	std::vector<float> FrameMilliseconds;
	std::string AssetStatus;
	bool bSceneViewer{};
	FFrameCaptureMetrics FrameCapture;
	FProfileStatus Profiling;
};

struct FDebugActions
{
	bool bSave{};
	bool bCapture{};
	bool bCaptureRdc{};
	bool bOpenRdc{};
	std::optional<std::uint32_t> ProfilingMask;
	std::optional<bool> Sampling;
	// Logical pixel bounds allow normalized input acceptance without OS input injection.
	FVec4 CaptureRdcBounds;
	FVec4 OpenRdcBounds;
	FVec4 AutoOpenRdcBounds;
	std::array<FVec4, 4> ProfilingBounds;
};

FDebugActions DrawFrameCaptureControls(FGui& InGui, FAppSettings& InSettings, const FFrameCaptureMetrics& InMetrics);
FDebugActions DrawProfilingControls(FGui& InGui, const FProfileStatus& InStatus);

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
	void BuildDeferred(FRenderGraph& InGraph, FGuiDrawData InData);

private:
	struct FImpl;
	std::shared_ptr<FImpl> Impl;
};

void RegisterDebugUiPlugin(FPluginRegistry& InRegistry, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                           FTaskSystem& InTasks, const FImage& InFont);
} // namespace Hyperion
