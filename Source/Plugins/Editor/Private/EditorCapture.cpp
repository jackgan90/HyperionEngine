#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/Capture/FrameCaptureScope.h"
#endif
#include <algorithm>

namespace Hyperion
{
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
	if (Gui->Checkbox("Enable RenderDoc capture", Options.Preferences.bRenderDocCapture))
	{
		bCaptureRequested = false;
		SavePreferences();
	}
	CapturePreferenceBounds = Gui->LastItemBounds();
	Gui->TextWrapped(CaptureStatus());
	Gui->TextWrapped("Preferences are saved locally. Enabling RenderDoc for the first time requires a restart.");
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
	PreferencesCloseBounds = Gui->LastItemBounds();
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
		bCaptureRequested = true;
	}
	CaptureButtonBounds = Gui->LastItemBounds();
	Gui->EndDisabled();
}

FImage FEditorPlugin::ExecuteEditorGraph(FRenderGraph InGraph, FSize InSize, bool bInScreenshot,
                                         FNativeSurface InSurface, bool bInCaptureRdc)
{
	const bool bVsync = !Options.bExercise && Options.Benchmark.empty();
#if HYP_ENABLE_RENDERDOC
	FImage Result;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&, Graph = std::move(InGraph), Capture = FrameCapture, InSurface, InSize, bInScreenshot,
	                           bVsync, bInCaptureRdc]() mutable
	                          {
		                          FFrameCaptureScope Scope(Tasks, Capture, InSurface, bInCaptureRdc);
		                          Result = ExecuteGraphOnRhi(std::move(Graph), Tasks, *Swapchain, InSize, bVsync,
		                                                     bInScreenshot);
		                          Scope.Finish(true);
	                          }));
	return Result;
#else
	(void)InSurface;
	(void)bInCaptureRdc;
	return ExecuteGraph(std::move(InGraph), Tasks, *Swapchain, InSize, bVsync, bInScreenshot);
#endif
}
} // namespace Hyperion
