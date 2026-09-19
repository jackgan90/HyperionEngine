#include "ViewerApplication.h"
#include <algorithm>

namespace Hyperion
{
void FViewerPlugin::InitializeCapture()
{
#if HYP_ENABLE_RENDERDOC
	Metrics.FrameCapture.bCompiled = true;
	Metrics.FrameCapture.Status = "RenderDoc disabled (enable plugin, save and restart)";
	FrameCapture = Context->Find<FFrameCapture>();
#endif
}

void FViewerPlugin::UpdateCaptureStatus()
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

bool FViewerPlugin::HandleCaptureActions(const FDebugActions& InActions, bool bInScheduled)
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

void FViewerPlugin::ExerciseCaptureInput(bool bInScheduled, std::vector<FInputEvent>& InEvents)
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
