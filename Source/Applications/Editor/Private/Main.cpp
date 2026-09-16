#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include <iostream>

namespace Hyperion
{
FEditorOptions ParseEditorOptions(int InCount, char** InValues)
{
	FEditorOptions Result;
	const auto Root = std::filesystem::path(HYP_SOURCE_DIR);
	Result.Mounts = Root / (std::filesystem::exists(Root / "ContentMounts.local.json") ? "ContentMounts.local.json"
	                                                                                   : "ContentMounts.json");
	Result.Layout = Root / "out/editor/Layout.ini";
	for (int Index = 1; Index < InCount; ++Index)
	{
		const std::string Argument = InValues[Index];
		if (Argument == "--hidden")
		{
			Result.bHidden = true;
			continue;
		}
		if (Argument == "--exercise")
		{
			Result.bExercise = true;
			continue;
		}
		if (Index + 1 >= InCount)
		{
			throw std::invalid_argument("Missing value for " + Argument);
		}
		const std::string Value = InValues[++Index];
		if (Argument == "--mounts")
		{
			Result.Mounts = Value;
		}
		else if (Argument == "--layout")
		{
			Result.Layout = Value;
		}
		else if (Argument == "--scene")
		{
			Result.Scene = Value;
		}
		else if (Argument == "--capture")
		{
			Result.Capture = Value;
		}
		else if (Argument == "--report")
		{
			Result.Report = Value;
		}
		else if (Argument == "--frames")
		{
			Result.Frames = static_cast<std::uint32_t>(std::stoul(Value));
		}
		else
		{
			throw std::invalid_argument("Unknown editor option: " + Argument);
		}
	}
	return Result;
}
} // namespace Hyperion

int main(int InCount, char** InValues)
{
	try
	{
		auto Options = Hyperion::ParseEditorOptions(InCount, InValues);
		Hyperion::InitializeLog(std::filesystem::path(HYP_SOURCE_DIR) / "out/logs/editor.log");
		{
			Hyperion::FEditorApplication Editor(std::move(Options));
			Editor.Run();
		}
		Hyperion::ShutdownLog();
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << "Hyperion Editor: " << Error.what() << '\n';
		return 1;
	}
}
