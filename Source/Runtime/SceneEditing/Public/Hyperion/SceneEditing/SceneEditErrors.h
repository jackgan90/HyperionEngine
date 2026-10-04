#pragma once
#include "Hyperion/Core/Errors/ErrorCode.h"

namespace Hyperion::SceneEditErrors
{
inline constexpr FErrorCodeId Busy{"busy"};
inline constexpr FErrorCodeId CaptureFailed{"capture_failed"};
inline constexpr FErrorCodeId ClipboardError{"clipboard_error"};
inline constexpr FErrorCodeId ClipboardUnavailable{"clipboard_unavailable"};
inline constexpr FErrorCodeId Conflict{"conflict"};
inline constexpr FErrorCodeId DirtyDocument{"dirty_document"};
inline constexpr FErrorCodeId InvalidArguments{"invalid_arguments"};
inline constexpr FErrorCodeId LoadFailed{"load_failed"};
inline constexpr FErrorCodeId NotFound{"not_found"};
inline constexpr FErrorCodeId ReadOnly{"read_only"};
inline constexpr FErrorCodeId SaveFailed{"save_failed"};
inline constexpr FErrorCodeId StaleDocument{"stale_document"};
inline constexpr FErrorCodeId StaleHandle{"stale_handle"};
inline constexpr FErrorCodeId StaleRevision{"stale_revision"};
inline constexpr FErrorCodeId Unavailable{"unavailable"};
} // namespace Hyperion::SceneEditErrors
