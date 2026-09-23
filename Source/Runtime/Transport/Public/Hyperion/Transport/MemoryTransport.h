#pragma once
#include "Hyperion/Transport/Transport.h"

namespace Hyperion
{
// Deterministic stream provider for protocol tests. Small limits force partial delivery/backpressure.
std::unique_ptr<ITransportProvider> MakeMemoryTransport(std::size_t InCapacity = 65536,
                                                        std::size_t InFragmentSize = 17);
} // namespace Hyperion
