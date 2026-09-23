#pragma once
#include "Hyperion/Automation/Connections.h"
#include <chrono>

namespace Hyperion
{
using FConnectionClock = std::chrono::steady_clock;
inline constexpr auto ConnectionTimeout = std::chrono::seconds(10);
inline constexpr std::uint32_t AutomationProtocolVersion = 1;

inline FArchiveNode CompletedConnectionResult(FArchiveNode InResult)
{
	return FArchiveNode(
	    FArchiveNode::FObject{{"status", WriteValue(std::string("completed"))}, {"result", std::move(InResult)}});
}

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
