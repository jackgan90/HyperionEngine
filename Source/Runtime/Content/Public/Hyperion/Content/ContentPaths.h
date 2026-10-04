#pragma once
#include <string_view>

namespace Hyperion
{
inline constexpr char GameContentRoot[] = "/Game";
inline constexpr char EngineContentRoot[] = "/Engine";

// Case-sensitive lexical membership, including the root itself. These do not normalize or validate paths.
// Callers retain IO normalization, traversal, mount, physical containment and permission checks.
bool IsGameContentPath(std::string_view InPath);
bool IsEngineContentPath(std::string_view InPath);
} // namespace Hyperion
