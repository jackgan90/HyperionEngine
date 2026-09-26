#pragma once
#include "Hyperion/Platform/Window.h"
#include <filesystem>
#include <optional>
#include <span>

namespace Hyperion
{
// Main-only native modal dialog. Cancel returns no value; errors throw.
std::optional<std::filesystem::path> SelectFolder(FNativeSurface InOwner, const std::filesystem::path& InInitial,
                                                  const std::string& InTitle = "Open asset root");

struct FFileDialogFilter
{
	std::string Name;
	std::string Pattern;
};

std::optional<std::filesystem::path> SelectFile(FNativeSurface InOwner, const std::filesystem::path& InInitial,
                                                std::span<const FFileDialogFilter> InFilters);
} // namespace Hyperion
