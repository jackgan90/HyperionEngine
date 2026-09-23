#pragma once
#include "Hyperion/Automation/Catalog.h"
#include "Hyperion/Content/ContentRootService.h"

namespace Hyperion
{
void RegisterContentRootOperations(FOperationCatalog& InCatalog, FContentRootService* InRoots,
                                   std::string InOwner = "automation-assets", bool bInMutable = true);
} // namespace Hyperion
