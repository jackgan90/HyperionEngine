#include "EditorPreferences.h"
#include "Hyperion/IO/IOService.h"
#include <sstream>

namespace Hyperion
{
FEditorPreferences LoadEditorPreferences(const std::filesystem::path& InPath)
{
	FLocalFileSystem Files;
	if (!Files.Exists(InPath))
	{
		return {};
	}
	const auto Bytes = Files.Read(InPath, 64 * 1024);
	std::istringstream Stream(std::string(reinterpret_cast<const char*>(Bytes.data()), Bytes.size()));
	FEditorPreferences Result;
	std::string Line;
	bool bVersion{};
	bool bCapture{};
	while (std::getline(Stream, Line))
	{
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.pop_back();
		}
		if (Line.empty())
		{
			continue;
		}
		const auto Separator = Line.find('=');
		const auto Key = Line.substr(0, Separator);
		const auto Value = Separator == std::string::npos ? std::string{} : Line.substr(Separator + 1);
		if (Key == "version" && Value == "1" && !bVersion)
		{
			bVersion = true;
		}
		else if (Key == "renderdoc_capture" && (Value == "true" || Value == "false") && !bCapture)
		{
			bCapture = true;
			Result.bRenderDocCapture = Value == "true";
		}
		else
		{
			throw std::runtime_error("Invalid editor preferences: " + Line);
		}
	}
	if (!bVersion || !bCapture)
	{
		throw std::runtime_error("Editor preferences are incomplete");
	}
	return Result;
}

void SaveEditorPreferences(const std::filesystem::path& InPath, const FEditorPreferences& InPreferences)
{
	const std::string Text =
	    std::string("version=1\nrenderdoc_capture=") + (InPreferences.bRenderDocCapture ? "true\n" : "false\n");
	FLocalFileSystem Files;
	Files.WriteAtomic(InPath, std::as_bytes(std::span(Text)));
}
} // namespace Hyperion
