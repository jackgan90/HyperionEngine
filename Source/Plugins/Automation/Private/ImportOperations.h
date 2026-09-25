#pragma once
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Automation/Catalog.h"

namespace Hyperion
{
void RegisterImportOperations(FOperationCatalog& InCatalog, FAssetImportWorkspace* InProvider);
void RegisterImportDraftOperations(FOperationCatalog& InCatalog, FAssetImportWorkspace* InProvider);
} // namespace Hyperion
