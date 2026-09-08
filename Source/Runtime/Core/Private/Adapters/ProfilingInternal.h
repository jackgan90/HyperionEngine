#pragma once
#include "Hyperion/Core/Profiling.h"
#if HYP_ENABLE_PROFILING
#include <tracy/TracyC.h>

namespace Hyperion
{
const ___tracy_source_location_data* ProfileSource(const FProfileSite& InSite);
}
#endif
