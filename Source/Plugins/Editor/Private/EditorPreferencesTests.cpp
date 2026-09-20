#include "EditorPreferences.h"
#include <fstream>
#include <iostream>
#include <source_location>

namespace
{
void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Editor preferences check failed at " + std::to_string(InLocation.line()));
	}
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		const auto Root = std::filesystem::current_path() / "editor-preferences-tests";
		std::filesystem::create_directories(Root);
		const auto Path = Root / "Preferences.ini";
		std::filesystem::remove(Path);
		Check(!LoadEditorPreferences(Path).bRenderDocCapture);
		SaveEditorPreferences(Path, {true});
		Check(LoadEditorPreferences(Path).bRenderDocCapture);
		SaveEditorPreferences(Path, {false});
		Check(!LoadEditorPreferences(Path).bRenderDocCapture);
		for (const std::string Text : {"", "version=2\nrenderdoc_capture=true", "version=1\nrenderdoc_capture=yes",
		                               "version=1\nrenderdoc_capture=true\nrenderdoc_capture=false"})
		{
			std::ofstream(Path) << Text;
			bool bFailed{};
			try
			{
				LoadEditorPreferences(Path);
			}
			catch (const std::exception&)
			{
				bFailed = true;
			}
			Check(bFailed);
		}
		SaveEditorPreferences(Path, {true});
		// A real replacement failure must leave the existing local preference intact.
		std::filesystem::permissions(Path, std::filesystem::perms::owner_read, std::filesystem::perm_options::replace);
		bool bFailed{};
		try
		{
			SaveEditorPreferences(Path, {false});
		}
		catch (const std::exception&)
		{
			bFailed = true;
		}
		std::filesystem::permissions(Path, std::filesystem::perms::owner_all, std::filesystem::perm_options::add);
		Check(bFailed && LoadEditorPreferences(Path).bRenderDocCapture);
		std::cout << "Editor preference defaults, round trips, malformed data and atomic failure passed\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
