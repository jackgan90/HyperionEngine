#pragma once
#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Config/ApplicationClose.h"
#include "Hyperion/Config/ApplicationSettings.h"
#include "Hyperion/GuiRenderer/Diagnostics.h"
#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/Renderer/ForwardRenderPipeline.h"
#include "Hyperion/Renderer/FramePipeline.h"
#include "Hyperion/Renderer/RenderCaptureControl.h"
#include "Hyperion/Renderer/RenderDiagnostics.h"
#include "Hyperion/Renderer/RenderOutput.h"
#include "Hyperion/Renderer/RenderPlugin.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneLightControls.h"
#include "Hyperion/Renderer/ShadowControls.h"
#include "ViewerOptions.h"
#include <deque>
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/Capture/FrameCapture.h"
#endif

namespace Hyperion
{
struct FViewerServices
{
	explicit FViewerServices(FPluginContext& InContext);
	~FViewerServices();
	void DrainWrites() noexcept;
	FTaskSystem& Tasks;
	FIOService& IO;
	FAssetService& Assets;
	std::vector<FTaskHandle> FileWrites;
};

// Main owns UI/input and consumes frame results. Graphics services outlive the drained CPU pipeline.
class FViewerPlugin final : public FPlugin,
                            public IRenderOutput,
                            public IRenderCaptureControl,
                            public IApplicationSettings,
                            public IRenderDiagnostics,
                            public IShadowControls,
                            public ISceneLightControls,
                            public IApplicationClose
{
public:
	FViewerPlugin(FOptions InOptions, FAppSettings InSettings);
	~FViewerPlugin();
	void Start(FPluginContext& InContext) override;
	void Update(const FPluginUpdate& InUpdate) override;
	void Quiesce() noexcept override;
	void Stop() noexcept override;
	std::shared_ptr<FPendingImageOutput> RequestImage(const FImageOutputRequest& InRequest) override;
	std::optional<FImageArtifact> PollImage(const std::shared_ptr<FPendingImageOutput>& InPending) override;
	FRenderCaptureInfo RenderCaptureInfo() const override;
	void RequestRenderCapture() override;
	void OpenRenderCapture() override;
	FApplicationSettingsState ApplicationSettings() const override;
	void EditApplicationSettings(std::uint64_t InRevision, const FAppSettings& InSettings) override;
	TAsyncResult<bool> SaveApplicationSettings(const std::filesystem::path& InPath) override;
	void ChangeProfiling(std::optional<std::uint32_t> InMask, std::optional<bool> InSampling) override;
	FRenderDiagnostics RenderDiagnostics() override;
	FSceneComponentDiagnostics ComponentDiagnostics(FSceneHandle InHandle, std::string_view InComponent) override;
	FCascadedShadowSettings ShadowControls() const override;
	void SetShadowControls(const FCascadedShadowSettings& InSettings) override;
	FSceneMainLight MainLight() override;
	void SetMainLight(const FSceneMainLight& InLight) override;
	FApplicationCloseState ApplicationCloseState() const override;
	FApplicationCloseState RequestApplicationClose(const FApplicationCloseRequest& InRequest) override;

private:
	std::string CloseState = "idle";
	std::string CloseError;
	std::optional<TAsyncResult<bool>> CloseSave;
	bool bCloseAfterSave{};
	void PollApplicationClose();
	std::shared_ptr<FPendingImageOutput> PendingImage;
	bool bAutomationRenderCapture{};
	void InitializeProfiling();
	void UpdateProfiling(int InFrame);
	void HandleProfilingActions(const FDebugActions& InActions);
	void InitializeCapture();
	void InitializePlugins();
	void ValidateRun();
	void Finish();
	void Tick(int InFrame, float InDelta);
	void PollInput();
	void ExerciseWindow(int InFrame);
	void ExerciseBenchmarkCamera(int InFrame);
	void SaveBenchmark();
	void MatchBenchmarkTimings();
	void InitializeShadowSettings();
	void UpdateShadowLight(int InFrame);
	void DrawShadowGui();
	void DrawMainLightGui();
	FSceneInstance* GetSceneInstance();
	void SetSceneLightDirection(FVec3 InDirection);
	FDebugActions BuildGui(int InFrame, float InDelta, FSize InLogical, FSize InPixels, FGuiDrawData& OutData);
	void ExerciseCaptureInput(bool bInScheduled, std::vector<FInputEvent>& InEvents);
	void ExerciseContactInput(std::vector<FInputEvent>& InEvents);
	FVec4 ContactShadowBounds;
	std::uint32_t ContactToggleCount{};
	std::uint32_t ContactStableFrames{};
	bool bContactMouseDown{};
	bool bContactExerciseCompleted{};
	std::uint32_t ContactWindowFrame{};
	bool bContactWindowCompleted{};
	FRenderFrame UpdateScene(FSize InSize);
	void RenderFrame(int InFrame, FSize InSize, FGuiDrawData InGuiData, bool bInTakeCapture, bool bInCaptureRdc);
	void CollectFrames();
	void DrainFrames();
	FRenderGraph BuildRenderGraph(const FRenderFrame& InFrame, const FGuiDrawData& InGuiData,
	                              std::shared_ptr<const FMaterialFrameContext> InMaterialFrame,
	                              std::shared_ptr<const FSceneFrameSeed> InSceneSeed,
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
	std::uint64_t SettingsRevision = 1;
	const bool bActiveReversedZ;
	FDebugMetrics Metrics;
#if HYP_ENABLE_RENDERDOC
	FFrameCapture* FrameCapture = nullptr;
#endif
	// Dependencies precede their consumers so partial initialization also unwinds safely.
	std::unique_ptr<FViewerServices> Services;
	FWindow* Window{};
	IRHIDevice* Device{};
	IRHISwapchain* Swapchain{};
	FShaderCompiler* Compiler{};
	FRenderSession* RenderSession{};
	std::unique_ptr<FSceneRenderPipeline> ScenePipeline;
	FCascadedShadowSettings ShadowSettings;
	FForwardPipelineStatistics PipelineStatistics;
	bool bShadowLightApplied{};
	FGui* Gui{};
	FPluginContext* Context{};
	FApplicationControl* Control{};
	FGuiRenderer* GuiRenderer{};
	IScenePlugin* SceneProducer{};
	IScenePlugin* ModelPlugin{};
	ISceneEditor* ScenePlugin{};
	float DeltaSeconds = 1.f / 60.f;
	bool bFinished{};
	bool bVerified{};
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
		std::shared_ptr<const FSceneFrameSeed> SceneSeed;
		FCascadedShadowSettings Shadows;
		FNativeSurface Surface;
		bool bTakeCapture{};
		bool bCaptureRdc{};
	};

	std::function<void()> PrepareFrame(FViewerFrameInput InInput, std::shared_ptr<FViewerFrameResult> InResult);

	struct FPendingViewerFrame
	{
		std::shared_ptr<FPendingImageOutput> ImageOutput;
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
