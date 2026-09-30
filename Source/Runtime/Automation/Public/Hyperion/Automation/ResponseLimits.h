#pragma once
#include "Hyperion/Reflection/Json.h"

namespace Hyperion
{
// A 1 MiB result may appear both as JSON and escaped text, alongside a request ID of up to 1 MiB.
// Keep request/result validation separate from bounded transport framing (including job/status wrappers).
inline constexpr FJsonLimits AutomationResponseLimits{4 * FJsonLimits{}.MaxBytes + 65536, FJsonLimits{}.MaxNodes + 256,
                                                      FJsonLimits{}.MaxDepth + 8};
} // namespace Hyperion
