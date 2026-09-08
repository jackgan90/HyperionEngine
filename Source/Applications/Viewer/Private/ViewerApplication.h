#pragma once
#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"
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
	void InitializeProfiling();
	void UpdateProfiling(int InFrame);
	void HandleProfilingActions(const FDebugActions& InActions);
	void InitializeCapture();
	void InitializeGraphics();
	void InitializePlugins();
	void RunFrames();
	void Tick(int InFrame, float InDelta);
	void PollInput();
	void ExerciseWindow(int InFrame);
	void ExerciseBenchmarkCamera(int InFrame);
	void SaveBenchmark();
	FDebugActions BuildGui(int InFrame, float InDelta, FSize InLogical, FSize InPixels, FGuiDrawData& OutData);
	void ExerciseCaptureInput(bool bInScheduled, std::vector<FInputEvent>& InEvents);
	FRenderFrame UpdateScene(FSize InSize);
	FImage RenderFrame(FSize InSize, const FGuiDrawData& InGuiData, bool bInTakeCapture);
	void UpdateCaptureStatus();
	void HandleCaptureActions(const FDebugActions& InActions, bool bInScheduled);
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
	std::unique_ptr<FRenderSession> RenderSession;
	std::unique_ptr<FGui> Gui;
	std::unique_ptr<FPluginSet> Plugins;
	FDebugUiPlugin* GuiPlugin{};
	FModelViewerPlugin* ModelPlugin{};
	FSceneViewerPlugin* ScenePlugin{};
	FSceneVisibilityStats SceneStatistics;

	struct FBenchmarkFrame
	{
		int Frame{};
		double Milliseconds{};
		std::size_t Draws{};
	};

	std::vector<FBenchmarkFrame> BenchmarkFrames;
	FVec4 RdcButtonBounds;
	bool bRdcMouseDown{};
	bool bCaptured{};
	bool bStopped{};
};
} // namespace Hyperion
