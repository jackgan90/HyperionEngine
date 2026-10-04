#include "EditorHostOptions.h"
#include "EditorOptions.h"
#include <array>
#include <iostream>
#include <source_location>

using namespace Hyperion;

namespace
{
struct FCaptureCase
{
	EEditorCaptureExercise Exercise;
	std::string_view Name;
};

constexpr std::array Cases{
    FCaptureCase{EEditorCaptureExercise::Toggle, "toggle"}, FCaptureCase{EEditorCaptureExercise::Capture, "capture"},
    FCaptureCase{EEditorCaptureExercise::Unavailable, "unavailable"}, FCaptureCase{EEditorCaptureExercise::Hud, "hud"}};

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Capture exercise check failed at line " + std::to_string(InLocation.line()));
	}
}

FEditorOptions ParseOptions(std::vector<std::string> InArguments)
{
	std::vector<char*> Arguments;
	for (auto& Argument : InArguments)
	{
		Arguments.push_back(Argument.data());
	}
	return ParseEditorOptions(static_cast<int>(Arguments.size()), Arguments.data());
}

void CheckRejected(std::vector<std::string> InArguments, std::string_view InMessage)
{
	bool bRejected{};
	try
	{
		ParseOptions(std::move(InArguments));
	}
	catch (const std::invalid_argument& Error)
	{
		Check(Error.what() == InMessage);
		bRejected = true;
	}
	Check(bRejected);
}

void CheckModesAndPolicy()
{
	const auto Default = ParseOptions({"Editor"});
	Check(!Default.ExerciseCapture && !HasEditorAcceptanceRequest(Default));
	Check(ShouldPersistEditorGui(Default) && ShouldPersistEditorContentLayout(Default));
	for (const auto& Case : Cases)
	{
		Check(ParseEditorCaptureExercise(Case.Name) == Case.Exercise);
		Check(EditorCaptureExerciseName(Case.Exercise) == Case.Name);
		const auto Options = ParseOptions(
		    {"Editor", "--exercise-capture", std::string(Case.Name), "--editor-preferences", "isolated.ini"});
		Check(Options.ExerciseCapture == Case.Exercise && Options.PreferencesPath == "isolated.ini");
		Check(HasEditorAcceptanceRequest(Options));
		Check(!ShouldPersistEditorGui(Options) && !ShouldPersistEditorContentLayout(Options));
		CheckRejected({"Editor", "--exercise-capture", std::string(Case.Name)},
		              "Capture acceptance requires an isolated --editor-preferences path");
		CheckRejected({"Editor", "--editor-preferences", Default.PreferencesPath.string(), "--exercise-capture",
		               std::string(Case.Name)},
		              "Capture acceptance requires an isolated --editor-preferences path");
	}
}

void CheckInvalidModes()
{
	constexpr std::string_view Message = "--exercise-capture expects toggle, capture, unavailable or hud";
	for (const std::string Name : {"", "other", "Capture", " capture", "hud ", "none"})
	{
		CheckRejected({"Editor", "--exercise-capture", Name}, Message);
		CheckRejected({"Editor", "--editor-preferences", "isolated.ini", "--exercise-capture", Name}, Message);
	}
	bool bRejected{};
	try
	{
		ParseEditorCaptureExercise(std::string_view("capture\0suffix", 14));
	}
	catch (const std::invalid_argument& Error)
	{
		Check(Error.what() == Message);
		bRejected = true;
	}
	Check(bRejected);
	bRejected = false;
	try
	{
		EditorCaptureExerciseName(static_cast<EEditorCaptureExercise>(999));
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	Check(bRejected);
}

void CheckArgumentOrder()
{
	CheckRejected({"Editor", "--exercise-capture"}, "Missing value for --exercise-capture");
	CheckRejected({"Editor", "--exercise-capture", "--hidden"},
	              "--exercise-capture expects toggle, capture, unavailable or hud");
	CheckRejected({"Editor", "--exercise-capture", "invalid", "--exercise-capture", "hud"},
	              "--exercise-capture expects toggle, capture, unavailable or hud");
	CheckRejected({"Editor", "--benchmark", "result.json", "--exercise-capture", "toggle"},
	              "Editor benchmark requires a scene and positive sample count; exercise is separate");
	CheckRejected({"Editor", "--profile", "--frames", "1", "--profile-start", "1", "--exercise-capture", "toggle"},
	              "Capture acceptance requires an isolated --editor-preferences path");
	CheckRejected({"Editor", "--profile", "--frames", "1", "--profile-start", "1", "--exercise-capture", "toggle",
	               "--editor-preferences", "isolated.ini"},
	              "Profiling window must fit within the finite frame limit");
	for (const auto& Last : Cases)
	{
		const auto Options =
		    ParseOptions({"Editor", "--editor-preferences", "first.ini", "--exercise-capture", "toggle",
		                  "--exercise-capture", std::string(Last.Name), "--editor-preferences", "last.ini"});
		Check(Options.ExerciseCapture == Last.Exercise && Options.PreferencesPath == "last.ini");
	}
}
} // namespace

int main()
{
	try
	{
		CheckModesAndPolicy();
		CheckInvalidModes();
		CheckArgumentOrder();
		std::cout << "PASS: typed capture exercises, CLI errors, ordering and GUI persistence policy\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
