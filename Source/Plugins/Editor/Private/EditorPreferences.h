#pragma once
#include <filesystem>

namespace Hyperion
{
struct FEditorPreferences
{
	bool bRenderDocCapture{};
};

FEditorPreferences LoadEditorPreferences(const std::filesystem::path& InPath);
void SaveEditorPreferences(const std::filesystem::path& InPath, const FEditorPreferences& InPreferences);
} // namespace Hyperion
