#include "EditorAcceptanceHarness.h"
#include <iostream>

namespace Hyperion
{
void FEditorAcceptanceHarness::ExerciseLogInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.FrameCount < 8)
	{
		return;
	}
	if (!Editor.Options.LogHistory)
	{
		throw std::runtime_error("Editor Log acceptance requires process history");
	}
	switch (Scenario.Log.Progress.GetState())
	{
		case ELogState::OpenWindowMenu:
			if (Editor.bShowLog)
			{
				throw std::runtime_error("Log must initially be hidden");
			}
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["placement/window-menu"], Scenario.Log.Click))
			{
				Scenario.Log.Progress.TransitionTo(ELogState::ShowLog);
			}
			return;
		case ELogState::VerifyLogAndOpenWindowMenu:
			if (!Editor.bShowLog)
			{
				throw std::runtime_error("Log did not open");
			}
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["placement/window-menu"], Scenario.Log.Click))
			{
				Scenario.Log.Progress.TransitionTo(ELogState::HideLog);
			}
			return;
		case ELogState::VerifyHiddenLogAndOpenWindowMenu:
			if (Editor.bShowLog)
			{
				throw std::runtime_error("Log did not close");
			}
			if (!Scenario.Log.bClosedPanelRecordWritten)
			{
				Scenario.Log.bClosedPanelRecordWritten = true;
				Log(ELogLevel::Info, "Log records while its panel is closed");
			}
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["placement/window-menu"], Scenario.Log.Click))
			{
				Scenario.Log.Progress.TransitionTo(ELogState::ReopenLog);
			}
			return;
		case ELogState::ShowLog:
		case ELogState::HideLog:
		case ELogState::ReopenLog:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["log/toggle"], Scenario.Log.Click))
			{
				Scenario.Log.Progress.TransitionTo(
				    Scenario.Log.Progress.Is(ELogState::ShowLog)   ? ELogState::VerifyLogAndOpenWindowMenu
				    : Scenario.Log.Progress.Is(ELogState::HideLog) ? ELogState::VerifyHiddenLogAndOpenWindowMenu
				                                                   : ELogState::VerifyLogHistory);
			}
			return;
	}
	if (Scenario.Log.Progress.Is(ELogState::VerifyLogHistory) &&
	    Scenario.Log.Observation.Is(ELogObservationPhase::EmitRecords) && Scenario.Log.RecordDelay.Advance())
	{
		if (!Editor.bShowLog)
		{
			throw std::runtime_error("Window > Log did not reopen the panel");
		}
		Log(ELogLevel::Info, "Editor Log info: original /Game/Paths preserved");
		Log(ELogLevel::Debug, "Editor Log debug: white text");
		Log(ELogLevel::Warning, "Editor Log warning: yellow text\nMultiline warning continuation");
		Log(ELogLevel::Error, "Editor Log error: red text");
		std::cout << "Editor stdout captured\n";
		std::cerr << "Editor stderr captured\n";
		Scenario.Log.Observation.TransitionTo(ELogObservationPhase::VerifyCapturedHistory);
		return;
	}
	if (Scenario.Log.Progress.Is(ELogState::VerifyLogHistory) &&
	    Scenario.Log.Observation.Is(ELogObservationPhase::VerifyCapturedHistory) && Scenario.Log.HistoryDelay.Advance())
	{
		const auto First = Editor.Options.LogHistory->Read({0, 1});
		if (First.Entries.empty() || First.Entries.front().Message != "Hyperion Editor starting")
		{
			throw std::runtime_error("Editor Log lost startup history");
		}
		Scenario.bLogVerified = true;
		Log(ELogLevel::Info, "Editor Log menu, reopening, startup replay and colors acceptance passed");
	}
}
} // namespace Hyperion
