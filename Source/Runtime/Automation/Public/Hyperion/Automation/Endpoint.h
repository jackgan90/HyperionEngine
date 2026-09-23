#pragma once
#include "Hyperion/Automation/Session.h"

namespace Hyperion
{
// A 1 MiB result may appear both as JSON and escaped text, alongside a request ID of up to 1 MiB.
// Keep request/result validation separate from bounded transport framing (including job/status wrappers).
inline constexpr FJsonLimits AutomationResponseLimits{4 * FJsonLimits{}.MaxBytes + 65536, FJsonLimits{}.MaxNodes + 256,
                                                      FJsonLimits{}.MaxDepth + 8};
std::string WriteAutomationResponse(const FArchiveNode& InValue);

// Fixed bootstrap methods. Domain operations are registered in the catalog, never here.
class FAutomationEndpoint
{
public:
	explicit FAutomationEndpoint(FAutomationSession& InSession);
	FArchiveNode Execute(std::string_view InMethod, const FArchiveNode& InParameters);
	FArchiveNode Tools() const;

private:
	FAutomationSession& Session;
};

class FMcpConnection
{
public:
	explicit FMcpConnection(FAutomationEndpoint& InEndpoint);
	// One JSON-RPC message, no newline. Notifications return no message.
	std::optional<std::string> Receive(std::string_view InMessage);

private:
	FArchiveNode Dispatch(std::string_view InMethod, const FArchiveNode& InParameters);
	FAutomationEndpoint& Endpoint;
	bool bInitialized{};
	bool bReady{};
};
} // namespace Hyperion
