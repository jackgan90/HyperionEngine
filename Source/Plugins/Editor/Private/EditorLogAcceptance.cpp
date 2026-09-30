#include "EditorApplication.h"
#include <iostream>

namespace Hyperion
{
void FEditorPlugin::ExerciseLogInput(std::vector<FInputEvent>& InEvents)
{
	if (FrameCount < 8)
	{
		return;
	}
	if (!Options.LogHistory)
	{
		throw std::runtime_error("Editor Log acceptance requires process history");
	}
	switch (Acceptance.ExerciseStep)
	{
		case 0:
			if (bShowLog)
			{
				throw std::runtime_error("Log must initially be hidden");
			}
			ExerciseClick(InEvents, InspectionBounds["placement/window-menu"]);
			return;
		case 2:
			if (!bShowLog)
			{
				throw std::runtime_error("Log did not open");
			}
			ExerciseClick(InEvents, InspectionBounds["placement/window-menu"]);
			return;
		case 4:
			if (bShowLog)
			{
				throw std::runtime_error("Log did not close");
			}
			if (!Acceptance.ExerciseWait)
			{
				Log(ELogLevel::Info, "Log records while its panel is closed");
			}
			ExerciseClick(InEvents, InspectionBounds["placement/window-menu"]);
			return;
		case 1:
		case 3:
		case 5:
			ExerciseClick(InEvents, InspectionBounds["log/toggle"]);
			return;
	}
	if (Acceptance.ExerciseStep == 6 && ++Acceptance.ExerciseWait == 4)
	{
		if (!bShowLog)
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
	if (Acceptance.ExerciseStep == 6 && Acceptance.ExerciseWait > 12)
	{
		const auto First = Options.LogHistory->Read({0, 1});
		if (First.Entries.empty() || First.Entries.front().Message != "Hyperion Editor starting")
		{
			throw std::runtime_error("Editor Log lost startup history");
		}
		Acceptance.bLogVerified = true;
		Log(ELogLevel::Info, "Editor Log menu, reopening, startup replay and colors acceptance passed");
	}
}
} // namespace Hyperion
