#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
void FEditorAcceptanceHarness::ExerciseCaptureHudInput(std::vector<FInputEvent>& InEvents)
{
	const auto Info = Editor.RenderCaptureHudInfo();
	if (Info.Enabled && Info.Enabled != Info.Preference)
	{
		throw std::runtime_error("HUD effective visibility differs from preference");
	}
	switch (Scenario.ExerciseStep)
	{
		case 0:
			ExerciseClick(InEvents, Scenario.EditMenuBounds);
			return;
		case 1:
			ExerciseClick(InEvents, Scenario.PreferencesMenuBounds);
			return;
		case 2:
			ExerciseClick(InEvents, Scenario.CaptureHudPreferenceBounds);
			return;
		case 3:
			ExerciseClick(InEvents, Scenario.PreferencesCloseBounds);
			return;
	}
	if (++Scenario.ExerciseWait < 3)
	{
		return;
	}
	if (Info.Preference == Scenario.bInitialCaptureHudPreference || Editor.bPreferencesDialog ||
	    LoadEditorPreferences(Editor.Options.PreferencesPath).bRenderDocHud != Info.Preference ||
	    Editor.Options.Preferences.bRenderDocCapture != Scenario.bInitialCapturePreference)
	{
		throw std::runtime_error("Editor HUD preference UI/persistence mismatch");
	}
	Log(ELogLevel::Info, "Editor capture acceptance passed: hud | " + Editor.CaptureStatus());
	Editor.Window->RequestClose();
}

void FEditorAcceptanceHarness::ExerciseCaptureInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.FrameCount == 0 && Editor.Options.ExerciseCapture == "capture")
	{
		// Fixed toolbar actions must remain reachable before the flexible view selector.
		Editor.Window->Resize({600, 960});
	}
	if (Editor.FrameCount < 8 || (Editor.Options.ExerciseCapture == "capture" && Editor.ReadyFrames < 8))
	{
		return;
	}
	if (Editor.Options.ExerciseCapture == "hud")
	{
		ExerciseCaptureHudInput(InEvents);
		return;
	}
	if (Editor.Options.ExerciseCapture == "toggle")
	{
		switch (Scenario.ExerciseStep)
		{
			case 0:
				ExerciseClick(InEvents, Scenario.EditMenuBounds);
				return;
			case 1:
				ExerciseClick(InEvents, Scenario.PreferencesMenuBounds);
				return;
			case 2:
				ExerciseClick(InEvents, Scenario.CapturePreferenceBounds);
				return;
			case 3:
				ExerciseClick(InEvents, Scenario.PreferencesCloseBounds);
				return;
		}
		if (++Scenario.ExerciseWait < 3)
		{
			return;
		}
		const bool bEnabled = Editor.Options.Preferences.bRenderDocCapture;
		const bool bVisible = Scenario.CaptureButtonBounds.Z > Scenario.CaptureButtonBounds.X;
		if (bEnabled == Scenario.bInitialCapturePreference || bVisible != bEnabled || Editor.bPreferencesDialog ||
		    LoadEditorPreferences(Editor.Options.PreferencesPath).bRenderDocCapture != bEnabled)
		{
			throw std::runtime_error("Editor capture preference UI/persistence mismatch");
		}
	}
	else if (Editor.Options.ExerciseCapture == "unavailable")
	{
		if (Editor.CanCapture() || Scenario.CaptureButtonBounds.Z <= Scenario.CaptureButtonBounds.X)
		{
			throw std::runtime_error("Unavailable capture must show a disabled button");
		}
	}
	else
	{
#if HYP_ENABLE_RENDERDOC
		if (Scenario.ExerciseStep == 0)
		{
			if (Scenario.CaptureButtonBounds.X < Editor.Viewport.ViewportRegion.Bounds.X ||
			    Scenario.CaptureButtonBounds.Z > Editor.Viewport.ViewportRegion.Bounds.Z)
			{
				throw std::runtime_error("Capture button is clipped by the narrow viewport");
			}
			if (!Editor.CanCapture())
			{
				throw std::runtime_error(Editor.CaptureStatus());
			}
			ExerciseClick(InEvents, Scenario.CaptureButtonBounds);
			return;
		}
		const auto Status = Editor.FrameCapture->Status();
		if (Editor.FrameCapture->OverlayEnabled() != Editor.Options.Preferences.bRenderDocHud)
		{
			throw std::runtime_error("Capture changed HUD visibility");
		}
		if (Status.CompletedCaptures != 1 || !Status.ReplayProcessId || Editor.RenderStats.MainView().Draws == 0)
		{
			throw std::runtime_error("Editor capture/replay failed: " + Editor.CaptureStatus());
		}
#else
		throw std::runtime_error("Capture exercise requires RenderDoc support");
#endif
	}
	Log(ELogLevel::Info,
	    "Editor capture acceptance passed: " + Editor.Options.ExerciseCapture + " | " + Editor.CaptureStatus());
	Editor.Window->RequestClose();
}
} // namespace Hyperion
