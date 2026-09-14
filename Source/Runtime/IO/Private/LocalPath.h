#pragma once
#include <filesystem>
#include <vector>

namespace Hyperion
{
void ValidateLocalPathCase(const std::filesystem::path& InRoot, const std::filesystem::path& InRelative);
std::vector<std::filesystem::path> EnumerateLocalContent(const std::filesystem::path& InDirectory, bool bInRecursive);
} // namespace Hyperion
