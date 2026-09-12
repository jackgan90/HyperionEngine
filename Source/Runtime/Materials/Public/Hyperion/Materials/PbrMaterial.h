#pragma once
#include "Hyperion/Materials/MaterialAsset.h"

namespace Hyperion
{
// CPU authoring preset used by importers and procedural clients. Native rendering consumes its stored result.
FMaterialAsset MakePbrMaterialAsset(std::string InName, EMaterialQueue InQueue = EMaterialQueue::Opaque,
                                    bool bInDoubleSided = false, bool bInUnlit = false);
} // namespace Hyperion
