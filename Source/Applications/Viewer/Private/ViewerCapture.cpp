#include "ViewerApplication.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/RenderDoc/RenderDocPlugin.h"
#endif
#include <algorithm>

namespace Hyperion
{
void FViewerApplication::InitializeCapture()
{
	const bool bEnabled =
	    std::find(Settings.Plugins.begin(), Settings.Plugins.end(), "renderdoc") != Settings.Plugins.end();
#if HYP_ENABLE_RENDERDOC
	Metrics.FrameCapture.bCompiled = true;
	Metrics.FrameCapture.Status = "RenderDoc disabled (enable plugin, save and restart)";
	if (bEnabled)
	{
		FPluginRegistry Registry;
		RegisterRenderDocPlugin(
		    Registry,
		    {std::filesystem::path(std::u8string(Settings.RenderDocLibrary.begin(), Settings.RenderDocLibrary.end())),
		     std::filesystem::path(std::u8string(Settings.RenderDocOutput.begin(), Settings.RenderDocOutput.end())),
		     Settings.ModelSource.empty() ? "Triangle" : "Model"});
		const std::array<std::string, 1> Ids{"renderdoc"};
		// Capture hooks must be installed before creating a window or any graphics objects.
		StartupPlugins = std::make_unique<FPluginSet>(Registry.Activate(Ids));
		for (const auto& Plugin : StartupPlugins->GetInstances())
		{
			FrameCapture = &static_cast<FRenderDocPlugin&>(*Plugin).Capture();
		}
	}
#else
	if (bEnabled)
	{
		throw std::runtime_error("RenderDoc plugin is not compiled; configure HYP_ENABLE_RENDERDOC=ON");
	}
#endif
}

void FViewerApplication::UpdateCaptureStatus()
{
#if HYP_ENABLE_RENDERDOC
	if (FrameCapture)
	{
		const auto Status = FrameCapture->Status();
		Metrics.FrameCapture.bAvailable = Status.bAvailable;
		Metrics.FrameCapture.bBusy = PendingCaptures != 0 || Status.State == EFrameCaptureState::Pending ||
		                             Status.State == EFrameCaptureState::Capturing;
		Metrics.FrameCapture.Status = Status.Message;
		const auto Path = Status.LastCapture.u8string();
		Metrics.FrameCapture.LastCapture.assign(reinterpret_cast<const char*>(Path.data()), Path.size());
		Metrics.FrameCapture.OpenStatus = Status.ReplayMessage;
	}
#endif
}

bool FViewerApplication::HandleCaptureActions(const FDebugActions& InActions, bool bInScheduled)
{
#if HYP_ENABLE_RENDERDOC
	if (FrameCapture && InActions.bOpenRdc)
	{
		FrameCapture->OpenLastCapture();
	}
	// The request travels with the target frame. Earlier queued frames cannot consume it.
	return FrameCapture && (InActions.bCaptureRdc || (bInScheduled && !Options.bExerciseRdcUi));
#else
	(void)InActions;
	(void)bInScheduled;
	return false;
#endif
}

void FViewerApplication::ExerciseCaptureInput(bool bInScheduled, std::vector<FInputEvent>& InEvents)
{
	if (Options.bExerciseRdcUi && (bInScheduled || bRdcMouseDown))
	{
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (RdcButtonBounds.X + RdcButtonBounds.Z) * .5f;
		Move.Y = (RdcButtonBounds.Y + RdcButtonBounds.W) * .5f;
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = bInScheduled;
		InEvents.push_back(Move);
		InEvents.push_back(Button);
		bRdcMouseDown = bInScheduled;
	}
}
} // namespace Hyperion
