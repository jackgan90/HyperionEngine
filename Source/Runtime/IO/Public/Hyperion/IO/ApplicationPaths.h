#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Hyperion
{
struct FStorageLaunchOptions
{
	std::string ApplicationId = "Editor";
	std::string Profile = "Default";
	std::filesystem::path SettingsFile;
	std::filesystem::path UserDataRoot;
	std::filesystem::path CacheRoot;
	bool bIsolated = false;
};

struct FApplicationPaths
{
	std::filesystem::path UserDataRoot;
	std::filesystem::path CacheRoot;
	std::filesystem::path Application;
	std::filesystem::path Config;
	std::filesystem::path State;
	std::filesystem::path Logs;
	std::filesystem::path Captures;
	std::filesystem::path Data;
};

std::string EnvironmentValue(std::string_view InName);
std::filesystem::path LocalUserDirectory();
std::filesystem::path ExecutableDirectory();
std::filesystem::path DefaultEngineContent(const std::filesystem::path& InDevelopmentDirectory = {});
std::filesystem::path InstalledConfigDirectory();
FStorageLaunchOptions ParseStorageLaunchOptions(int InCount, char** InValues, std::string InApplicationId);
bool IsStorageValueArgument(std::string_view InArgument);
bool IsStorageFlagArgument(std::string_view InArgument);
void ValidateStorageIdentity(std::string_view InValue);
FApplicationPaths MakeApplicationPaths(const FStorageLaunchOptions& InOptions,
                                       const std::filesystem::path& InUserDataRoot,
                                       const std::filesystem::path& InCacheRoot);
// Case-insensitive Windows containment, including equality. Both inputs must be normalized absolute paths.
bool PathContains(const std::filesystem::path& InParent, const std::filesystem::path& InChild);
// Rejects symbolic links/junctions in existing path components; missing suffixes are allowed.
void RequireUnlinkedPath(const std::filesystem::path& InPath);
} // namespace Hyperion
