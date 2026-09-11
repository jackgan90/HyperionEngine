#pragma once
#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "Hyperion/Renderer/ForwardRenderPipeline.h"
#include "Hyperion/Renderer/FramePipeline.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"
#include "ViewerOptions.h"
#include <deque>
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

// Main owns UI/input and consumes frame results. Graphics services outlive the drained CPU pipeline.
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
	void MatchBenchmarkTimings();
	void InitializeShadowSettings();
	void UpdateShadowLight(int InFrame);
	void DrawShadowGui();
	FDebugActions BuildGui(int InFrame, float InDelta, FSize InLogical, FSize InPixels, FGuiDrawData& OutData);
	void ExerciseCaptureInput(bool bInScheduled, std::vector<FInputEvent>& InEvents);
	FRenderFrame UpdateScene(FSize InSize);
	void RenderFrame(int InFrame, FSize InSize, FGuiDrawData InGuiData, bool bInTakeCapture, bool bInCaptureRdc);
	void CollectFrames();
	void DrainFrames();
	FRenderGraph BuildRenderGraph(const FRenderFrame& InFrame, const FGuiDrawData& InGuiData,
	                              std::shared_ptr<const FMaterialFrameContext> InMaterialFrame,
	                              const FCascadedShadowSettings& InShadows);
	void UpdateCaptureStatus();
	bool HandleCaptureActions(const FDebugActions& InActions, bool bInScheduled);
	void SaveSettingsAsync(const std::filesystem::path& InPath);
	void SaveScreenshot(FImage InImage, const FAppSettings& InSettings, const std::string& InSceneError);
	void VerifyOutputs();
	FDeviceStats ReleaseGraphics();
	void Shutdown();

	FOptions Options;
	FAppSettings Settings;
	const bool bActiveReversedZ;
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
	std::unique_ptr<FSceneRenderPipeline> ScenePipeline;
	FCascadedShadowSettings ShadowSettings;
	FForwardPipelineStatistics PipelineStatistics;
	FVec3 ShadowLight;
	float LightAzimuth{};
	float LightElevation{};
	std::unique_ptr<FGui> Gui;
	std::unique_ptr<FPluginSet> Plugins;
	FDebugUiPlugin* GuiPlugin{};
	FModelViewerPlugin* ModelPlugin{};
	FSceneViewerPlugin* ScenePlugin{};
	FSceneVisibilityStats SceneStatistics;

	struct FViewerFrameResult
	{
		FForwardPipelineStatistics Pipeline;
		FDeviceStats Device;
		FImage Screenshot;
		std::uint64_t CompletedAt{};
	};

	struct FViewerFrameInput
	{
		FRenderFrame Frame;
		std::uint64_t FrameId{};
		FGuiDrawData Gui;
		std::shared_ptr<const FMaterialFrameContext> Material;
		FCascadedShadowSettings Shadows;
		FNativeSurface Surface;
		bool bTakeCapture{};
		bool bCaptureRdc{};
	};

	std::function<void()> PrepareFrame(FViewerFrameInput InInput, std::shared_ptr<FViewerFrameResult> InResult);

	struct FPendingViewerFrame
	{
		FFrameTicket Ticket;
		std::shared_ptr<FViewerFrameResult> Result;
		FAppSettings Settings;
		std::string SceneError;
		int Frame{};
		std::uint64_t StartedAt{};
		double MainMilliseconds{};
		bool bTakeCapture{};
		bool bCaptureRdc{};
		bool bBenchmark{};
	};

	// Declared after every referenced graphics service; drain also runs explicitly before teardown.
	std::unique_ptr<FFramePipeline> FramePipeline;
	std::deque<FPendingViewerFrame> PendingFrames;
	std::uint64_t FrameStartedAt{};
	std::size_t PendingCaptures{};

	struct FBenchmarkFrame
	{
		int Frame{};
		double Milliseconds{};
		std::size_t Draws{};
		std::size_t VisibleItems{};
		FRenderBatchStats Batches;
		FForwardPipelineStatistics Pipeline;
		FDeviceStats Device;
		double CpuLatencyMilliseconds{};
	};

	std::vector<FBenchmarkFrame> BenchmarkFrames;
	FGpuTimingCapture BenchmarkGpu;
	FVec4 RdcButtonBounds;
	bool bRdcMouseDown{};
	bool bCaptured{};
	bool bStopped{};
};
} // namespace Hyperion
