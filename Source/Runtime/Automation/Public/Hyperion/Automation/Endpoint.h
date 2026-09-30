#pragma once
#include "Hyperion/Automation/ResponseLimits.h"
#include "Hyperion/Automation/Session.h"

namespace Hyperion
{
std::string WriteAutomationResponse(const FArchiveNode& InValue);
FArchiveNode AutomationTools();

struct FEndpointRequest
{
	// Main-owned, one terminal value. Poll never pumps Main or blocks on IO.
	std::function<std::optional<FArchiveNode>()> Poll;
};

FEndpointRequest ReadyAutomationRequest(FArchiveNode InValue);

class IAutomationEndpoint
{
public:
	virtual ~IAutomationEndpoint() = default;
	virtual FEndpointRequest Begin(std::string_view InMethod, const FArchiveNode& InParameters) = 0;
	virtual FArchiveNode Tools() const = 0;
};

// Fixed bootstrap methods. Domain operations are registered in the catalog, never here.
class FAutomationEndpoint : public IAutomationEndpoint
{
public:
	explicit FAutomationEndpoint(FAutomationSession& InSession);
	FArchiveNode Execute(std::string_view InMethod, const FArchiveNode& InParameters);
	FEndpointRequest Begin(std::string_view InMethod, const FArchiveNode& InParameters) override;
	FArchiveNode Tools() const override;

private:
	FAutomationSession& Session;
};

class FMcpConnection
{
public:
	explicit FMcpConnection(IAutomationEndpoint& InEndpoint);
	// One JSON-RPC message, no newline. Notifications return no message.
	std::optional<std::string> Receive(std::string_view InMessage);
	std::vector<std::string> Poll();
	bool HasPending() const;

private:
	FArchiveNode Dispatch(std::string_view InMethod, const FArchiveNode& InParameters);
	FEndpointRequest BeginTool(const FArchiveNode& InParameters);

	struct FPending
	{
		FArchiveNode Id;
		FEndpointRequest Request;
	};

	IAutomationEndpoint& Endpoint;
	std::vector<FPending> Pending;
	bool bInitialized{};
	bool bReady{};
};
} // namespace Hyperion
