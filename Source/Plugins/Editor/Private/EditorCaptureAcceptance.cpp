#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
void FEditorPlugin::ExerciseCaptureHudInput(std::vector<FInputEvent>& InEvents)
{
	const auto Info = RenderCaptureHudInfo();
	if (Info.Enabled && Info.Enabled != Info.Preference)
	{
		throw std::runtime_error("HUD effective visibility differs from preference");
	}
	switch (ExerciseStep)
	{
		case 0:
			ExerciseClick(InEvents, EditMenuBounds);
			return;
		case 1:
			ExerciseClick(InEvents, PreferencesMenuBounds);
			return;
		case 2:
			ExerciseClick(InEvents, CaptureHudPreferenceBounds);
			return;
		case 3:
			ExerciseClick(InEvents, PreferencesCloseBounds);
			return;
	}
	if (++ExerciseWait < 3)
	{
		return;
	}
	if (Info.Preference == bInitialCaptureHudPreference || bPreferencesDialog ||
	    LoadEditorPreferences(Options.PreferencesPath).bRenderDocHud != Info.Preference ||
	    Options.Preferences.bRenderDocCapture != bInitialCapturePreference)
	{
		throw std::runtime_error("Editor HUD preference UI/persistence mismatch");
	}
	Log(ELogLevel::Info, "Editor capture acceptance passed: hud | " + CaptureStatus());
	Window->RequestClose();
}

void FEditorPlugin::ExerciseCaptureInput(std::vector<FInputEvent>& InEvents)
{
	if (FrameCount == 0 && Options.ExerciseCapture == "capture")
	{
		// Fixed toolbar actions must remain reachable before the flexible view selector.
		Window->Resize({600, 960});
	}
	if (FrameCount < 8 || (Options.ExerciseCapture == "capture" && ReadyFrames < 8))
	{
		return;
	}
	if (Options.ExerciseCapture == "hud")
	{
		ExerciseCaptureHudInput(InEvents);
		return;
	}
	if (Options.ExerciseCapture == "toggle")
	{
		switch (ExerciseStep)
		{
			case 0:
				ExerciseClick(InEvents, EditMenuBounds);
				return;
			case 1:
				ExerciseClick(InEvents, PreferencesMenuBounds);
				return;
			case 2:
				ExerciseClick(InEvents, CapturePreferenceBounds);
				return;
			case 3:
				ExerciseClick(InEvents, PreferencesCloseBounds);
				return;
		}
		if (++ExerciseWait < 3)
		{
			return;
		}
		const bool bEnabled = Options.Preferences.bRenderDocCapture;
		const bool bVisible = CaptureButtonBounds.Z > CaptureButtonBounds.X;
		if (bEnabled == bInitialCapturePreference || bVisible != bEnabled || bPreferencesDialog ||
		    LoadEditorPreferences(Options.PreferencesPath).bRenderDocCapture != bEnabled)
		{
			throw std::runtime_error("Editor capture preference UI/persistence mismatch");
		}
	}
	else if (Options.ExerciseCapture == "unavailable")
	{
		if (CanCapture() || CaptureButtonBounds.Z <= CaptureButtonBounds.X)
		{
			throw std::runtime_error("Unavailable capture must show a disabled button");
		}
	}
	else
	{
#if HYP_ENABLE_RENDERDOC
		if (ExerciseStep == 0)
		{
			if (CaptureButtonBounds.X < ViewportRegion.Bounds.X || CaptureButtonBounds.Z > ViewportRegion.Bounds.Z)
			{
				throw std::runtime_error("Capture button is clipped by the narrow viewport");
			}
			if (!CanCapture())
			{
				throw std::runtime_error(CaptureStatus());
			}
			ExerciseClick(InEvents, CaptureButtonBounds);
			return;
		}
		const auto Status = FrameCapture->Status();
		if (FrameCapture->OverlayEnabled() != Options.Preferences.bRenderDocHud)
		{
			throw std::runtime_error("Capture changed HUD visibility");
		}
		if (Status.CompletedCaptures != 1 || !Status.ReplayProcessId || RenderStats.MainView().Draws == 0)
		{
			throw std::runtime_error("Editor capture/replay failed: " + CaptureStatus());
		}
#else
		throw std::runtime_error("Capture exercise requires RenderDoc support");
#endif
	}
	Log(ELogLevel::Info, "Editor capture acceptance passed: " + Options.ExerciseCapture + " | " + CaptureStatus());
	Window->RequestClose();
}
} // namespace Hyperion
