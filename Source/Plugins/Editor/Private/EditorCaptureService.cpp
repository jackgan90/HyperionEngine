#include "EditorApplication.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
FRenderCaptureInfo FEditorPlugin::RenderCaptureInfo() const
{
	FRenderCaptureInfo Result;
	Result.bRunning = !bFinished && !bStopped;
	Result.Preference = Options.Preferences.bRenderDocCapture;
	Result.Message = CaptureStatus();
	Result.bBusy = bCaptureRequested;
#if HYP_ENABLE_RENDERDOC
	Result.bCompiled = true;
	if (FrameCapture)
	{
		const auto Status = FrameCapture->Status();
		Result.bAvailable = Status.bAvailable && Options.Preferences.bRenderDocCapture;
		Result.bBusy |= Status.State == EFrameCaptureState::Pending || Status.State == EFrameCaptureState::Capturing;
		Result.bFailed = !bCaptureRequested && Status.State == EFrameCaptureState::Failed;
		Result.Completed = Status.CompletedCaptures;
		Result.Path = PathToUtf8(Status.LastCapture);
		Result.ReplayMessage = Status.ReplayMessage;
	}
#endif
	return Result;
}

void FEditorPlugin::RequestRenderCapture()
{
	if (!CanCapture() || bFinished || Window->Minimized())
	{
		throw FSceneEditError(SceneEditErrors::Unavailable, CaptureStatus());
	}
	bCaptureRequested = true;
}

void FEditorPlugin::OpenRenderCapture()
{
#if HYP_ENABLE_RENDERDOC
	if (FrameCapture && FrameCapture->OpenLastCapture())
	{
		return;
	}
#endif
	throw FSceneEditError(SceneEditErrors::Unavailable, CaptureStatus());
}

FRenderCaptureHudInfo FEditorPlugin::RenderCaptureHudInfo() const
{
	FRenderCaptureHudInfo Result;
	Result.Preference = Options.Preferences.bRenderDocHud;
#if HYP_ENABLE_RENDERDOC
	if (FrameCapture)
	{
		Result.Enabled = FrameCapture->OverlayEnabled();
	}
#endif
	return Result;
}

void FEditorPlugin::SetRenderCaptureHudPreference(bool bInEnabled)
{
	auto Preferences = Options.Preferences;
	Preferences.bRenderDocHud = bInEnabled;
	try
	{
		SaveEditorPreferences(Options.PreferencesPath, Preferences);
	}
	catch (const std::exception& Error)
	{
		Options.PreferenceError = "Could not save editor preferences: " + std::string(Error.what());
		throw FSceneEditError(SceneEditErrors::SaveFailed, Options.PreferenceError);
	}
	Options.Preferences = std::move(Preferences);
	Options.PreferenceError.clear();
#if HYP_ENABLE_RENDERDOC
	if (FrameCapture)
	{
		FrameCapture->SetOverlayEnabled(bInEnabled);
	}
#endif
}

void FEditorPlugin::SetRenderCapturePreference(bool bInEnabled)
{
	if (RenderCaptureInfo().bBusy)
	{
		throw FSceneEditError(SceneEditErrors::Busy,
		                      "Wait for the accepted capture before changing capture preferences");
	}
	Options.Preferences.bRenderDocCapture = bInEnabled;
	bCaptureRequested = false;
	SavePreferences();
	if (!Options.PreferenceError.empty())
	{
		throw FSceneEditError(SceneEditErrors::SaveFailed, Options.PreferenceError);
	}
}
} // namespace Hyperion
