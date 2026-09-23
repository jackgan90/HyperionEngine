#pragma once
#include "Hyperion/Automation/Catalog.h"
#include "Hyperion/SceneEditing/SceneRequests.h"

namespace Hyperion
{
void RegisterSceneOperations(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument);
} // namespace Hyperion
