#pragma once
#include "Hyperion/RHI/RHIBackend.h"

namespace Hyperion
{
// Used only by application/test composition roots. No native SDK types escape.
void RegisterD3D12RHIBackend(FRHIBackendRegistry& InRegistry);
} // namespace Hyperion
