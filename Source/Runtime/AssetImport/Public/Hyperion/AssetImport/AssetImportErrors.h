#pragma once
#include "Hyperion/Core/Errors/ErrorCode.h"

namespace Hyperion::AssetImportErrors
{
inline constexpr FErrorCodeId Busy{"busy"};
inline constexpr FErrorCodeId Dirty{"dirty"};
inline constexpr FErrorCodeId InvalidArguments{"invalid_arguments"};
inline constexpr FErrorCodeId NotFound{"not_found"};
inline constexpr FErrorCodeId ReadOnly{"read_only"};
inline constexpr FErrorCodeId StaleRevision{"stale_revision"};
inline constexpr FErrorCodeId Unavailable{"unavailable"};
} // namespace Hyperion::AssetImportErrors
