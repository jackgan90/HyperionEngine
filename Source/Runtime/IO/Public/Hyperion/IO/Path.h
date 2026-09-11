#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace Hyperion
{
std::filesystem::path PathFromUtf8(std::string_view InPath);
std::string PathToUtf8(const std::filesystem::path& InPath);
} // namespace Hyperion
