#pragma once
#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "ViewerOptions.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/Capture/FrameCapture.h"
#endif

namespace Hyperion
{
struct FViewerServices
{
	explicit FViewerServices(const FAppSettings& InSettings);
	~FViewerServices();
	void DrainWrites() noexcept;
	FTaskSystem Tasks;
	FIOService IO;
	FAssetService Assets;
	std::vector<FTaskHandle> FileWrites;
};

// Owns one Viewer session. Main waits for each Render frame before mutating input/settings.
class FViewerApplication
{
public:
	explicit FViewerApplication(FOptions InOptions);
	~FViewerApplication();
	void Run();

private:
	void InitializeCapture();
	void InitializeGraphics();
	void InitializePlugins();
	void RunFrames();
	void Tick(int InFrame, float InDelta);
	void PollInput();
	void ExerciseWindow(int InFrame);
	FDebugActions BuildGui(int InFrame, float InDelta, FSize InLogical, FSize InPixels, FGuiDrawData& OutData);
	void ExerciseCaptureInput(bool InScheduled, std::vector<FInputEvent>& InEvents);
	FImage RenderFrame(FSize InSize, const FGuiDrawData& InGuiData, bool InTakeCapture);
	void UpdateCaptureStatus();
	void HandleCaptureActions(const FDebugActions& InActions, bool InScheduled);
	void SaveSettingsAsync(const std::filesystem::path& InPath);
	void SaveScreenshot(FImage InImage);
	void VerifyOutputs();
	FDeviceStats ReleaseGraphics();
	void Shutdown();

	FOptions Options;
	FAppSettings Settings;
	FDebugMetrics Metrics;
#if HYP_ENABLE_RENDERDOC
	std::unique_ptr<FPluginSet> StartupPlugins;
	FFrameCapture* FrameCapture = nullptr;
#endif
	// Dependencies precede their consumers so partial initialization also unwinds safely.
	std::unique_ptr<FViewerServices> Services;
	std::unique_ptr<FWindow> Window;
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FShaderCompiler> Compiler;
	std::unique_ptr<FGui> Gui;
	std::unique_ptr<FPluginSet> Plugins;
	FDebugUiPlugin* GuiPlugin{};
	FModelViewerPlugin* ModelPlugin{};
	FVec4 RdcButtonBounds;
	bool RdcMouseDown{};
	bool Captured{};
	bool Stopped{};
};
} // namespace Hyperion
