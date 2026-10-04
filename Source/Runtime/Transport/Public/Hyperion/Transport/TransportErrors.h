#pragma once
#include "Hyperion/Core/Errors/ErrorCode.h"

namespace Hyperion::TransportErrors
{
inline constexpr FErrorCodeId AccessDenied{"access_denied"};
inline constexpr FErrorCodeId AddressInUse{"address_in_use"};
inline constexpr FErrorCodeId Backpressure{"backpressure"};
inline constexpr FErrorCodeId ConnectionFailed{"connection_failed"};
inline constexpr FErrorCodeId Disconnected{"disconnected"};
inline constexpr FErrorCodeId DiscoveryUnavailable{"discovery_unavailable"};
inline constexpr FErrorCodeId InvalidAddress{"invalid_address"};
inline constexpr FErrorCodeId ListenFailed{"listen_failed"};
inline constexpr FErrorCodeId ProtocolError{"protocol_error"};
inline constexpr FErrorCodeId TransportFailed{"transport_failed"};
inline constexpr FErrorCodeId UnsupportedTransport{"unsupported_transport"};
} // namespace Hyperion::TransportErrors
