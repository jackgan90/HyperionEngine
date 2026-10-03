#pragma once
#include "Hyperion/Automation/Connections.h"
#include <chrono>

namespace Hyperion
{
using FConnectionClock = std::chrono::steady_clock;
inline constexpr auto ConnectionTimeout = std::chrono::seconds(10);
inline constexpr auto ConnectionDrainTimeout = std::chrono::seconds(2);
inline constexpr std::size_t MaxServerPeers = 32;
inline constexpr FAutomationLimits PeerSessionLimits{.MaxRunningJobs = 8, .MaxRetainedJobs = 32};
inline constexpr std::size_t MaxHandshakeBytes = 8192;
inline constexpr std::size_t MaxCorrelationIdBytes = 64;
inline constexpr unsigned MaxRequestsPerPeerPoll = 4;
inline constexpr unsigned MaxRemoteConnectionRequests = 32;
inline constexpr std::size_t MaxRemoteConnections = 16;
inline constexpr std::uint32_t DefaultProbeTimeoutMs = 1000;
inline constexpr std::uint32_t MinProbeTimeoutMs = 50;
inline constexpr std::uint32_t MaxProbeTimeoutMs = 5000;
inline constexpr std::uint32_t AutomationProtocolVersion = 1;

inline FArchiveNode CurrentConnectionFailure()
{
	try
	{
		throw;
	}
	catch (const FTransportError& Error)
	{
		return AutomationFailure(Error.GetCode(), Error.what());
	}
	catch (...)
	{
		return CurrentAutomationFailure();
	}
}

inline const FArchiveNode::FObject& ConnectionFields(const FArchiveNode& InValue)
{
	const auto* Fields = std::get_if<FArchiveNode::FObject>(&InValue.Value);
	if (!Fields)
	{
		throw FAutomationError("invalid_arguments", "Expected an object");
	}
	return *Fields;
}

inline void CheckConnectionKeys(const FArchiveNode::FObject& InFields,
                                std::initializer_list<std::string_view> InAllowed)
{
	for (const auto& [Key, Value] : InFields)
	{
		if (std::find(InAllowed.begin(), InAllowed.end(), Key) == InAllowed.end())
		{
			throw FAutomationError("invalid_arguments", "Unknown field", Key);
		}
	}
}
} // namespace Hyperion
