#include "Hyperion/IO/Path.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "ViewerApplication.h"
#include <algorithm>
#include <utility>

namespace Hyperion
{
FRenderCaptureInfo FViewerPlugin::RenderCaptureInfo() const
{
	FRenderCaptureInfo Result;
	Result.bRunning = !bFinished && !bStopped;
	Result.bBusy = bAutomationRenderCapture || PendingCaptures != 0;
	Result.Message = "RenderDoc is not available; enable the compiled plugin at startup.";
#if HYP_ENABLE_RENDERDOC
	Result.bCompiled = true;
	if (FrameCapture)
	{
		const auto Status = FrameCapture->Status();
		Result.bAvailable = Status.bAvailable;
		Result.bBusy |= Status.State == EFrameCaptureState::Pending || Status.State == EFrameCaptureState::Capturing;
		Result.bFailed = !Result.bBusy && Status.State == EFrameCaptureState::Failed;
		Result.Completed = Status.CompletedCaptures;
		Result.Path = PathToUtf8(Status.LastCapture);
		Result.Message = Status.Message;
		Result.ReplayMessage = Status.ReplayMessage;
	}
#endif
	return Result;
}

void FViewerPlugin::RequestRenderCapture()
{
	const auto Status = RenderCaptureInfo();
	if (!Status.bAvailable || !Status.bRunning || Status.bBusy || Window->Minimized())
	{
		throw FSceneEditError("unavailable", Status.Message);
	}
	bAutomationRenderCapture = true;
}

void FViewerPlugin::OpenRenderCapture()
{
#if HYP_ENABLE_RENDERDOC
	if (FrameCapture && FrameCapture->OpenLastCapture())
	{
		return;
	}
#endif
	throw FSceneEditError("unavailable", RenderCaptureInfo().Message);
}

std::shared_ptr<FPendingImageOutput> FViewerPlugin::RequestImage(const FImageOutputRequest& InRequest)
{
	if (InRequest.Window != "main")
	{
		throw std::invalid_argument("Viewer supports the main window only");
	}
	if (PendingImage || bFinished || Window->Minimized())
	{
		throw FSceneEditError("busy", "Wait for a drawable application and pending capture");
	}
	PendingImage = PrepareImageOutput(InRequest);
	return PendingImage;
}

std::optional<FImageArtifact> FViewerPlugin::PollImage(const std::shared_ptr<FPendingImageOutput>& InPending)
{
	CollectFrames();
	if (!InPending->Error.empty())
	{
		throw FSceneEditError("capture_failed", InPending->Error);
	}
	if (!InPending->Result && (bFinished || bStopped))
	{
		throw FSceneEditError("unavailable", "Application stopped before capture");
	}
	return InPending->Result;
}

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
	return FrameCapture && (std::exchange(bAutomationRenderCapture, false) || InActions.bCaptureRdc ||
	                        (bInScheduled && !Options.bExerciseRdcUi));
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
