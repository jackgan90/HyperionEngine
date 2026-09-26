#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace Hyperion
{
std::filesystem::path PathFromUtf8(std::string_view InPath);
std::string PathToUtf8(const std::filesystem::path& InPath);
// Package paths and paths on another filesystem root retain their absolute identity.
std::string PathRelativeToUtf8(const std::filesystem::path& InPath, const std::filesystem::path& InBase);
// Native case-insensitive collision key; retain original spelling for actual filesystem access.
std::string PathCaseKey(const std::filesystem::path& InPath);
bool IsPackagePath(const std::filesystem::path& InPath);
std::filesystem::path NormalizeFilePath(const std::filesystem::path& InPath);
} // namespace Hyperion
