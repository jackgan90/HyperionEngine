#include "Hyperion/Core/Core.h"
#include "ViewerApplication.h"

namespace Hyperion
{
void FViewerApplication::SaveSettingsAsync(const std::filesystem::path& InPath)
{
	const auto Text = EncodeReflected(SettingsType(), &Settings);
	const auto Bytes = std::as_bytes(std::span(Text));
	Services->FileWrites.push_back(Services->IO.WriteAsync(InPath, {Bytes.begin(), Bytes.end()}).Task());
}

void FViewerApplication::SaveScreenshot(FImage InImage, const FAppSettings& InSettings, const std::string& InSceneError)
{
	if (Options.bVerifyModel && !InSceneError.empty())
	{
		throw std::runtime_error(InSceneError);
	}
	const auto Path = Options.Capture.empty() ? std::filesystem::path(HYP_SOURCE_DIR) / "out/captures" /
	                                                ("capture-" + std::to_string(ClockNanoseconds()) + ".png")
	                                          : Options.Capture;
	VerifyImage(InImage, InSettings, Options);
	Services->FileWrites.push_back(
	    DispatchAsync<bool>(Services->Tasks, {EDomain::Worker},
	                        [Services = Services.get(), Path, Snapshot = std::move(InImage)]
	                        {
		                        return *Services->IO.WriteAsync(Path, EncodePng(Snapshot)).Get(Services->Tasks);
	                        })
	        .Task());
	bCaptured = true;
	Log(ELogLevel::Info, "Screenshot save queued: " + Path.string());
}

void FViewerApplication::VerifyOutputs()
{
	Log(ELogLevel::Info, std::string("Depth convention: active=") + (bActiveReversedZ ? "reversed" : "standard") +
	                         "; configured=" + (Settings.bReversedZ ? "reversed" : "standard"));
	const auto Progress = FramePipeline->Progress();
	Log(ELogLevel::Info, "CPU frames: submitted=" + std::to_string(Progress.Submitted) +
	                         "; render=" + std::to_string(Progress.RenderCompleted) +
	                         "; rhi=" + std::to_string(Progress.RhiCompleted) +
	                         "; leads=" + std::to_string(Metrics.FrameLimits.MainLead) + "," +
	                         std::to_string(Metrics.FrameLimits.RenderLead));
	if (ScenePlugin)
	{
		Log(ELogLevel::Info, "Scene: " + ScenePlugin->Status() + " | groups=" + std::to_string(SceneStatistics.Groups) +
		                         " visits=" + std::to_string(SceneStatistics.VisitedNodes) +
		                         " group_tests=" + std::to_string(SceneStatistics.GroupTests) +
		                         " collects=" + std::to_string(SceneStatistics.CollectedPrimitives) +
		                         " items=" + std::to_string(SceneStatistics.VisibleItems) +
		                         " draws=" + std::to_string(SceneStatistics.Draws));
	}
	if (!Options.Capture.empty() && !bCaptured)
	{
		throw std::runtime_error("Requested capture was not produced");
	}
#if HYP_ENABLE_RENDERDOC
	if (!Options.RdcFrames.empty() &&
	    (!FrameCapture || FrameCapture->Status().CompletedCaptures != Options.RdcFrames.size()))
	{
		throw std::runtime_error("Requested RDC capture was not produced: " +
		                         (FrameCapture ? FrameCapture->Status().Message : std::string("plugin unavailable")));
	}
#endif
	if (!Options.SaveConfig.empty())
	{
		const auto Size = Window->LogicalSize();
		Settings.Width = static_cast<int>(Size.Width);
		Settings.Height = static_cast<int>(Size.Height);
		SaveSettingsAsync(Options.SaveConfig);
	}
}
} // namespace Hyperion
