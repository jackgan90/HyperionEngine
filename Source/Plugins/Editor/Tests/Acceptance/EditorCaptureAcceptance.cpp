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
	switch (Scenario.CapturePreference.Progress.GetState())
	{
		case ECapturePreferenceState::OpenEditMenu:
			if (ExerciseClick(InEvents, Scenario.EditMenuBounds, Scenario.CapturePreference.Click))
			{
				Scenario.CapturePreference.Progress.TransitionTo(ECapturePreferenceState::OpenPreferences);
			}
			return;
		case ECapturePreferenceState::OpenPreferences:
			if (ExerciseClick(InEvents, Scenario.PreferencesMenuBounds, Scenario.CapturePreference.Click))
			{
				Scenario.CapturePreference.Progress.TransitionTo(ECapturePreferenceState::TogglePreference);
			}
			return;
		case ECapturePreferenceState::TogglePreference:
			if (ExerciseClick(InEvents, Scenario.CaptureHudPreferenceBounds, Scenario.CapturePreference.Click))
			{
				Scenario.CapturePreference.Progress.TransitionTo(ECapturePreferenceState::ClosePreferences);
			}
			return;
		case ECapturePreferenceState::ClosePreferences:
			if (ExerciseClick(InEvents, Scenario.PreferencesCloseBounds, Scenario.CapturePreference.Click))
			{
				Scenario.CapturePreference.Progress.TransitionTo(ECapturePreferenceState::VerifyPreference);
			}
			return;
	}
	if (!Scenario.CapturePreference.PreferenceObservation.Advance())
	{
		return;
	}
	if (Info.Preference == Scenario.bInitialCaptureHudPreference || Editor.bPreferencesDialog ||
	    LoadEditorPreferences(Editor.Options.PreferencesPath).bRenderDocHud != Info.Preference ||
	    Editor.Options.Preferences.bRenderDocCapture != Scenario.bInitialCapturePreference)
	{
		throw std::runtime_error("Editor HUD preference UI/persistence mismatch");
	}
	Log(ELogLevel::Info,
	    "Editor capture acceptance passed: " + std::string(EditorCaptureExerciseName(EEditorCaptureExercise::Hud)) +
	        " | " + Editor.CaptureStatus());
	Editor.Window->RequestClose();
}

void FEditorAcceptanceHarness::ExerciseCaptureInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.FrameCount == 0 && Editor.Options.ExerciseCapture == EEditorCaptureExercise::Capture)
	{
		// Fixed toolbar actions must remain reachable before the flexible view selector.
		Editor.Window->Resize({600, 960});
	}
	if (Editor.FrameCount < 8 ||
	    (Editor.Options.ExerciseCapture == EEditorCaptureExercise::Capture && Editor.ReadyFrames < 8))
	{
		return;
	}
	if (Editor.Options.ExerciseCapture == EEditorCaptureExercise::Hud)
	{
		ExerciseCaptureHudInput(InEvents);
		return;
	}
	if (Editor.Options.ExerciseCapture == EEditorCaptureExercise::Toggle)
	{
		switch (Scenario.CapturePreference.Progress.GetState())
		{
			case ECapturePreferenceState::OpenEditMenu:
				if (ExerciseClick(InEvents, Scenario.EditMenuBounds, Scenario.CapturePreference.Click))
				{
					Scenario.CapturePreference.Progress.TransitionTo(ECapturePreferenceState::OpenPreferences);
				}
				return;
			case ECapturePreferenceState::OpenPreferences:
				if (ExerciseClick(InEvents, Scenario.PreferencesMenuBounds, Scenario.CapturePreference.Click))
				{
					Scenario.CapturePreference.Progress.TransitionTo(ECapturePreferenceState::TogglePreference);
				}
				return;
			case ECapturePreferenceState::TogglePreference:
			{
				const bool bCompleted =
				    ExerciseClick(InEvents, Scenario.CapturePreferenceBounds, Scenario.CapturePreference.Click);
				Scenario.CapturePreference.bCaptureBeforeToggle =
				    Scenario.CapturePreference.Click.ReachedPressBoundary();
				if (bCompleted)
				{
					Scenario.CapturePreference.Progress.TransitionTo(ECapturePreferenceState::ClosePreferences);
				}
				return;
			}
			case ECapturePreferenceState::ClosePreferences:
				if (ExerciseClick(InEvents, Scenario.PreferencesCloseBounds, Scenario.CapturePreference.Click))
				{
					Scenario.CapturePreference.Progress.TransitionTo(ECapturePreferenceState::VerifyPreference);
				}
				return;
		}
		if (!Scenario.CapturePreference.PreferenceObservation.Advance())
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
	else if (Editor.Options.ExerciseCapture == EEditorCaptureExercise::Unavailable)
	{
		if (Editor.CanCapture() || Scenario.CaptureButtonBounds.Z <= Scenario.CaptureButtonBounds.X)
		{
			throw std::runtime_error("Unavailable capture must show a disabled button");
		}
	}
	else
	{
#if HYP_ENABLE_RENDERDOC
		if (Scenario.Capture.Progress.Is(ECaptureState::CaptureFrame))
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
			if (ExerciseClick(InEvents, Scenario.CaptureButtonBounds, Scenario.Capture.Click))
			{
				Scenario.Capture.Progress.TransitionTo(ECaptureState::VerifyCapture);
			}
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
	    "Editor capture acceptance passed: " + std::string(EditorCaptureExerciseName(*Editor.Options.ExerciseCapture)) +
	        " | " + Editor.CaptureStatus());
	Editor.Window->RequestClose();
}
} // namespace Hyperion
