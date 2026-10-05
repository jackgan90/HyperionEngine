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
	switch (Scenario.ExerciseStep)
	{
		case 0:
			if (Editor.bShowLog)
			{
				throw std::runtime_error("Log must initially be hidden");
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds["placement/window-menu"]);
			return;
		case 2:
			if (!Editor.bShowLog)
			{
				throw std::runtime_error("Log did not open");
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds["placement/window-menu"]);
			return;
		case 4:
			if (Editor.bShowLog)
			{
				throw std::runtime_error("Log did not close");
			}
			if (!Scenario.ExerciseWait)
			{
				Log(ELogLevel::Info, "Log records while its panel is closed");
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds["placement/window-menu"]);
			return;
		case 1:
		case 3:
		case 5:
			ExerciseClick(InEvents, Scenario.InspectionBounds["log/toggle"]);
			return;
	}
	if (Scenario.ExerciseStep == 6 && ++Scenario.ExerciseWait == 4)
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
	}
	if (Scenario.ExerciseStep == 6 && Scenario.ExerciseWait > 12)
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
