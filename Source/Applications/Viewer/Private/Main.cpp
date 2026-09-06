#include "Hyperion/Core/Core.h"
#include "ViewerApplication.h"
#include <iostream>

int main(int InArgCount, char** InArgValues)
{
	try
	{
		auto Options = Hyperion::ParseOptions(InArgCount, InArgValues);
		Hyperion::InitializeLog(std::filesystem::path(HYP_SOURCE_DIR) / "out/logs/viewer.log");
		{
			Hyperion::FViewerApplication Application(std::move(Options));
			Application.Run();
		}
		Hyperion::ShutdownLog();
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << "Hyperion: " << Error.what() << '\n';
		return 1;
	}
}
