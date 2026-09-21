#include "EditorPreferences.h"
#include "Hyperion/IO/IOService.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>
#include <cwctype>
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
	bool bRoot{};
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
		else if (Key == "asset_root" && !bRoot)
		{
			bRoot = true;
			Result.AssetRoot = PathFromUtf8(Value);
		}
		else if (Key == "recent_root" && !Value.empty() && Result.RecentRoots.size() < 5)
		{
			Result.RecentRoots.push_back(PathFromUtf8(Value));
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
	std::string Text =
	    std::string("version=1\nrenderdoc_capture=") + (InPreferences.bRenderDocCapture ? "true\n" : "false\n");
	Text += "asset_root=" + PathToUtf8(InPreferences.AssetRoot) + "\n";
	for (std::size_t Index = 0; Index < std::min<std::size_t>(5, InPreferences.RecentRoots.size()); ++Index)
	{
		Text += "recent_root=" + PathToUtf8(InPreferences.RecentRoots[Index]) + "\n";
	}
	FLocalFileSystem Files;
	Files.WriteAtomic(InPath, std::as_bytes(std::span(Text)));
}

bool SameAssetRoot(const std::filesystem::path& InFirst, const std::filesystem::path& InSecond)
{
	auto First = InFirst.lexically_normal().native();
	auto Second = InSecond.lexically_normal().native();
	std::transform(First.begin(), First.end(), First.begin(), std::towlower);
	std::transform(Second.begin(), Second.end(), Second.begin(), std::towlower);
	return First == Second;
}

void RememberAssetRoot(FEditorPreferences& InPreferences, const std::filesystem::path& InRoot)
{
	InPreferences.AssetRoot = InRoot;
	std::erase_if(InPreferences.RecentRoots,
	              [&](const auto& InPrevious)
	              {
		              return SameAssetRoot(InPrevious, InRoot);
	              });
	InPreferences.RecentRoots.insert(InPreferences.RecentRoots.begin(), InRoot);
	if (InPreferences.RecentRoots.size() > 5)
	{
		InPreferences.RecentRoots.resize(5);
	}
}
} // namespace Hyperion
