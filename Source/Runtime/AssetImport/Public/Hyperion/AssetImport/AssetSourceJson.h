#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"

namespace Hyperion
{
// Diagnostic JSON export of the reflected record tree; not an import source format.
std::string EncodeAssetSourceJson(const FArchiveNode& InNode);
} // namespace Hyperion
