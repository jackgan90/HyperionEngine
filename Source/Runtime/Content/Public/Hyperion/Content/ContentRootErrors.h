#pragma once
#include "Hyperion/Core/Errors/ErrorCode.h"

namespace Hyperion::ContentRootErrors
{
inline constexpr FErrorCodeId Busy{"busy"};
inline constexpr FErrorCodeId ContentFailed{"content_failed"};
inline constexpr FErrorCodeId DirtyDocument{"dirty_document"};
inline constexpr FErrorCodeId InvalidRoot{"invalid_root"};
inline constexpr FErrorCodeId RootUnset{"root_unset"};
inline constexpr FErrorCodeId StaleRevision{"stale_revision"};
} // namespace Hyperion::ContentRootErrors
