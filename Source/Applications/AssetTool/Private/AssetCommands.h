#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"
#include <ostream>

namespace Hyperion
{
void RunAssetCommand(std::span<const std::string_view> InArguments, FIOService& InIO, std::ostream& InOutput);
} // namespace Hyperion
