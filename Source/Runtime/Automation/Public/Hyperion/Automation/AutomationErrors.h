#pragma once
#include "Hyperion/Core/Errors/ErrorCode.h"

namespace Hyperion::AutomationErrors
{
inline constexpr FErrorCodeId AccessDenied{"access_denied"};
inline constexpr FErrorCodeId Busy{"busy"};
inline constexpr FErrorCodeId Cancelled{"cancelled"};
inline constexpr FErrorCodeId CaptureFailed{"capture_failed"};
inline constexpr FErrorCodeId DirtyDocument{"dirty_document"};
inline constexpr FErrorCodeId Disconnected{"disconnected"};
inline constexpr FErrorCodeId DiscoveryUnavailable{"discovery_unavailable"};
inline constexpr FErrorCodeId InternalError{"internal_error"};
inline constexpr FErrorCodeId InvalidArguments{"invalid_arguments"};
inline constexpr FErrorCodeId InvalidRequest{"invalid_request"};
inline constexpr FErrorCodeId InvalidRoot{"invalid_root"};
inline constexpr FErrorCodeId LoadFailed{"load_failed"};
inline constexpr FErrorCodeId NotCancellable{"not_cancellable"};
inline constexpr FErrorCodeId NotFound{"not_found"};
inline constexpr FErrorCodeId OperationFailed{"operation_failed"};
inline constexpr FErrorCodeId ProtocolError{"protocol_error"};
inline constexpr FErrorCodeId ProtocolMismatch{"protocol_mismatch"};
inline constexpr FErrorCodeId ReadOnly{"read_only"};
inline constexpr FErrorCodeId ResultUnavailable{"result_unavailable"};
inline constexpr FErrorCodeId SaveFailed{"save_failed"};
inline constexpr FErrorCodeId SessionClosed{"session_closed"};
inline constexpr FErrorCodeId StaleDocument{"stale_document"};
inline constexpr FErrorCodeId StaleRevision{"stale_revision"};
inline constexpr FErrorCodeId StaleTarget{"stale_target"};
inline constexpr FErrorCodeId Timeout{"timeout"};
inline constexpr FErrorCodeId Unavailable{"unavailable"};
inline constexpr FErrorCodeId UnsupportedType{"unsupported_type"};
} // namespace Hyperion::AutomationErrors
