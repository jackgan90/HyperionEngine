#pragma once
#include "Hyperion/Core/Errors/ErrorCode.h"

namespace Hyperion::AssetWorkflowErrors
{
inline constexpr FErrorCodeId Busy{"busy"};
inline constexpr FErrorCodeId StaleDocument{"stale_document"};
inline constexpr FErrorCodeId StaleRevision{"stale_revision"};
inline constexpr FErrorCodeId UnsupportedType{"unsupported_type"};
} // namespace Hyperion::AssetWorkflowErrors
