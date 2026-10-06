#include "Hyperion/IO/ApplicationPaths.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
void ValidateStorageIdentity(std::string_view InValue)
{
	if (InValue.empty() || InValue.size() > 64 || InValue == "." || InValue == ".." ||
	    !std::all_of(InValue.begin(), InValue.end(),
	                 [](char InChar)
	                 {
		                 return (InChar >= 'a' && InChar <= 'z') || (InChar >= 'A' && InChar <= 'Z') ||
		                        (InChar >= '0' && InChar <= '9') || InChar == '-' || InChar == '_';
	                 }))
	{
		throw std::invalid_argument("Storage identity requires 1-64 ASCII letters, digits, '-' or '_'");
	}
}

bool IsStorageValueArgument(std::string_view InArgument)
{
	return InArgument == "--user-data-root" || InArgument == "--cache-root" || InArgument == "--storage-settings" ||
	       InArgument == "--storage-profile";
}

bool IsStorageFlagArgument(std::string_view InArgument)
{
	return InArgument == "--isolated-storage";
}

FStorageLaunchOptions ParseStorageLaunchOptions(int InCount, char** InValues, std::string InApplicationId)
{
	FStorageLaunchOptions Result;
	Result.ApplicationId = std::move(InApplicationId);
	Result.SettingsFile = PathFromUtf8(EnvironmentValue("HYP_STORAGE_SETTINGS"));
	Result.UserDataRoot = PathFromUtf8(EnvironmentValue("HYP_USER_DATA_ROOT"));
	Result.CacheRoot = PathFromUtf8(EnvironmentValue("HYP_CACHE_ROOT"));
	const auto Profile = EnvironmentValue("HYP_STORAGE_PROFILE");
	if (!Profile.empty())
	{
		Result.Profile = Profile;
	}
	Result.bIsolated = EnvironmentValue("HYP_ISOLATED_STORAGE") == "1";
	for (int Index = 1; Index < InCount; ++Index)
	{
		const std::string_view Argument(InValues[Index]);
		if (IsStorageFlagArgument(Argument))
		{
			Result.bIsolated = true;
		}
		else if (IsStorageValueArgument(Argument))
		{
			if (++Index >= InCount || std::string_view(InValues[Index]).empty())
			{
				throw std::invalid_argument("Missing value for " + std::string(Argument));
			}
			const std::string Value(InValues[Index]);
			if (Argument == "--storage-profile")
			{
				Result.Profile = Value;
			}
			else if (Argument == "--storage-settings")
			{
				Result.SettingsFile = PathFromUtf8(Value);
			}
			else if (Argument == "--user-data-root")
			{
				Result.UserDataRoot = PathFromUtf8(Value);
			}
			else
			{
				Result.CacheRoot = PathFromUtf8(Value);
			}
		}
	}
	ValidateStorageIdentity(Result.ApplicationId);
	ValidateStorageIdentity(Result.Profile);
	return Result;
}

FApplicationPaths MakeApplicationPaths(const FStorageLaunchOptions& InOptions,
                                       const std::filesystem::path& InUserDataRoot,
                                       const std::filesystem::path& InCacheRoot)
{
	ValidateStorageIdentity(InOptions.ApplicationId);
	ValidateStorageIdentity(InOptions.Profile);
	FApplicationPaths Result;
	Result.UserDataRoot = NormalizeFilePath(InUserDataRoot);
	Result.CacheRoot = NormalizeFilePath(InCacheRoot);
	Result.Application = Result.UserDataRoot / "Apps" / InOptions.ApplicationId / InOptions.Profile;
	Result.Config = Result.Application / "Config";
	Result.State = Result.Application / "State";
	Result.Logs = Result.Application / "Logs";
	Result.Captures = Result.Application / "Captures";
	Result.Data = Result.Application / "Data";
	if (PathContains(Result.CacheRoot, Result.Application) || PathContains(Result.Application, Result.CacheRoot))
	{
		throw std::invalid_argument("Cache root must not overlap application configuration, state or user data");
	}
	return Result;
}

bool PathContains(const std::filesystem::path& InParent, const std::filesystem::path& InChild)
{
	const auto Parent = PathCaseKey(InParent);
	const auto Child = PathCaseKey(InChild);
	return Parent == Child ||
	       (Child.starts_with(Parent) &&
	        (Parent.ends_with('/') || (Child.size() > Parent.size() && Child[Parent.size()] == '/')));
}

std::filesystem::path DefaultEngineContent(const std::filesystem::path& InDevelopmentDirectory)
{
	const auto Executable = ExecutableDirectory();
	for (const auto& Candidate : {Executable / "Content", Executable.parent_path() / "Content"})
	{
		if (std::filesystem::is_directory(Candidate))
		{
			return Candidate;
		}
	}
	if (!InDevelopmentDirectory.empty())
	{
		return NormalizeFilePath(InDevelopmentDirectory);
	}
	return Executable / "Content";
}

std::filesystem::path InstalledConfigDirectory()
{
	const auto Executable = ExecutableDirectory();
	const auto Adjacent = Executable / "Config";
	return std::filesystem::is_directory(Adjacent) ? Adjacent : Executable.parent_path() / "Config";
}
} // namespace Hyperion
