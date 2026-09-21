#pragma once
#include "Hyperion/Assets/AssetService.h"
#include <ostream>

namespace Hyperion
{
void MigrateNativeLibrary(FIOService& InIO, const std::filesystem::path& InRoot, const std::filesystem::path& InOutput,
                          std::ostream& InLog);
void ValidateNativeLibrary(FIOService& InIO, const std::filesystem::path& InRoot, std::ostream& InLog);
} // namespace Hyperion
