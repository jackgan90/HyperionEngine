#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/Capture/FrameCaptureScope.h"
#endif
#include <algorithm>

namespace Hyperion
{
std::shared_ptr<FPendingImageOutput> FEditorPlugin::RequestImage(const FImageOutputRequest& InRequest)
{
	if (PendingImage || bFinished || Window->Minimized())
	{
		throw FSceneEditError("busy", "Wait for the pending screenshot and a drawable application");
	}
	if (InRequest.Window != "main" && InRequest.Window != "assets")
	{
		throw std::invalid_argument("Window must be main or assets");
	}
	if (InRequest.Window == "assets" && (!AssetWindow || !AssetWindow->IsDrawable()))
	{
		throw FSceneEditError("busy", "Open a drawable asset window");
	}
	PendingImage = PrepareImageOutput(InRequest);
	if (InRequest.Window == "assets")
	{
		AssetWindow->RequestImage(PendingImage);
	}
	return PendingImage;
}

std::optional<FImageArtifact> FEditorPlugin::PollImage(const std::shared_ptr<FPendingImageOutput>& InPending)
{
	if (!InPending->Error.empty())
	{
		throw FSceneEditError("capture_failed", InPending->Error);
	}
	if (!InPending->Result && InPending->Request.Window == "assets" && !AssetWindow)
	{
		throw FSceneEditError("unavailable", "Asset window closed before capture");
	}
	if (!InPending->Result && (bFinished || bStopped))
	{
		throw FSceneEditError("unavailable", "Application stopped before capture");
	}
	return InPending->Result;
}

bool FEditorPlugin::CanCapture() const
{
#if HYP_ENABLE_RENDERDOC
	if (Options.Preferences.bRenderDocCapture && FrameCapture && !bCaptureRequested)
	{
		const auto Status = FrameCapture->Status();
		return Status.bAvailable && Status.State != EFrameCaptureState::Pending &&
		       Status.State != EFrameCaptureState::Capturing;
	}
#endif
	return false;
}

std::string FEditorPlugin::CaptureStatus() const
{
	if (!Options.Preferences.bRenderDocCapture)
	{
		return "RenderDoc capture is disabled.";
	}
#if HYP_ENABLE_RENDERDOC
	if (std::ranges::find(Options.DisabledPlugins, "renderdoc") != Options.DisabledPlugins.end())
	{
		return "RenderDoc was explicitly disabled at startup.";
	}
	if (!FrameCapture)
	{
		return "Restart the editor to enable RenderDoc capture.";
	}
	const auto Status = FrameCapture->Status();
	return Status.Message + (Status.ReplayMessage.empty() ? "" : " | " + Status.ReplayMessage);
#else
	return "RenderDoc support is not compiled in. Build with HYP_ENABLE_RENDERDOC=ON and restart.";
#endif
}

void FEditorPlugin::SavePreferences()
{
	try
	{
		SaveEditorPreferences(Options.PreferencesPath, Options.Preferences);
		Options.PreferenceError.clear();
	}
	catch (const std::exception& Failure)
	{
		Options.PreferenceError = "Could not save editor preferences: " + std::string(Failure.what());
		Log(ELogLevel::Warning, Options.PreferenceError);
	}
}

void FEditorPlugin::DrawPreferences()
{
	if (bRequestPreferences)
	{
		Gui->OpenPopup("Editor preference");
		bRequestPreferences = false;
	}
	if (!bPreferencesDialog || !Gui->BeginModal("Editor preference", bPreferencesDialog))
	{
		return;
	}
	bool bEnabled = Options.Preferences.bRenderDocCapture;
	if (Gui->Checkbox("Enable RenderDoc capture (requires restart)", bEnabled))
	{
		try
		{
			SetRenderCapturePreference(bEnabled);
		}
		catch (const std::exception&)
		{ /* SavePreferences retains the message displayed below. */
		}
	}
	Acceptance.ObserveWidget(EEditorWidget::CapturePreference, Gui->LastItemBounds());
	bool bHudEnabled = Options.Preferences.bRenderDocHud;
	if (Gui->Checkbox("Show RenderDoc HUD", bHudEnabled))
	{
		try
		{
			SetRenderCaptureHudPreference(bHudEnabled);
		}
		catch (const FSceneEditError&)
		{ /* The shared operation retains the persistence error displayed below. */
		}
	}
	Acceptance.ObserveWidget(EEditorWidget::CaptureHudPreference, Gui->LastItemBounds());
	if (!Options.PreferenceError.empty())
	{
		Gui->TextWrapped(Options.PreferenceError);
		if (Gui->Button("Retry saving preferences"))
		{
			SavePreferences();
		}
	}
	if (Gui->Button("Close"))
	{
		Gui->ClosePopup();
		bPreferencesDialog = false;
	}
	Acceptance.ObserveWidget(EEditorWidget::PreferencesClose, Gui->LastItemBounds());
	Gui->EndModal();
}

void FEditorPlugin::DrawCaptureButton()
{
	if (!Options.Preferences.bRenderDocCapture)
	{
		return;
	}
	Gui->SameLine();
	Gui->BeginDisabled(!CanCapture());
	const auto Tip = "Capture frame and open in RenderDoc\n" + CaptureStatus();
	if (Gui->IconButton("##RenderDocCapture", EGuiIcon::Capture, Tip.c_str()))
	{
		RequestRenderCapture();
	}
	Acceptance.ObserveWidget(EEditorWidget::CaptureButton, Gui->LastItemBounds());
	Gui->EndDisabled();
}

FImage FEditorPlugin::ExecuteEditorGraph(FRenderGraph InGraph, FSize InSize, bool bInScreenshot,
                                         FNativeSurface InSurface, bool bInCaptureRdc, bool bInVsync)
{
#if HYP_ENABLE_RENDERDOC
	FImage Result;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&, Graph = std::move(InGraph), Capture = FrameCapture, InSurface, InSize, bInScreenshot,
	                           bInVsync, bInCaptureRdc]() mutable
	                          {
		                          FFrameCaptureScope Scope(Tasks, Capture, InSurface, bInCaptureRdc);
		                          Result = ExecuteGraphOnRhi(std::move(Graph), Tasks, *Swapchain, InSize, bInVsync,
		                                                     bInScreenshot);
		                          Scope.Finish(true);
	                          }));
	return Result;
#else
	(void)InSurface;
	(void)bInCaptureRdc;
	return ExecuteGraph(std::move(InGraph), Tasks, *Swapchain, InSize, bInVsync, bInScreenshot);
#endif
}
} // namespace Hyperion
