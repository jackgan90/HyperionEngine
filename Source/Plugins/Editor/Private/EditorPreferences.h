#pragma once
#include <filesystem>
#include <vector>

namespace Hyperion
{
struct FEditorPreferences
{
	bool bRenderDocCapture{};
	std::filesystem::path AssetRoot;
	std::vector<std::filesystem::path> RecentRoots;
};

bool SameAssetRoot(const std::filesystem::path& InFirst, const std::filesystem::path& InSecond);
void RememberAssetRoot(FEditorPreferences& InPreferences, const std::filesystem::path& InRoot);

FEditorPreferences LoadEditorPreferences(const std::filesystem::path& InPath);
void SaveEditorPreferences(const std::filesystem::path& InPath, const FEditorPreferences& InPreferences);
} // namespace Hyperion
