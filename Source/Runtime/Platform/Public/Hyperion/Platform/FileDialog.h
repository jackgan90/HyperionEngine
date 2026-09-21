#pragma once
#include "Hyperion/Platform/Window.h"
#include <filesystem>
#include <optional>

namespace Hyperion
{
// Main-only native modal dialog. Cancel returns no value; errors throw.
std::optional<std::filesystem::path> SelectFolder(FNativeSurface InOwner, const std::filesystem::path& InInitial);
} // namespace Hyperion
