#pragma once
#include "Hyperion/Automation/Channel.h"
#include "Hyperion/Automation/Endpoint.h"
#include "Hyperion/Automation/Targets.h"

namespace Hyperion
{
// Owns per-peer sessions, not domain services. Poll only at a host's safe Main update boundary.
class FAutomationServer
{
public:
	FAutomationServer(const FOperationCatalog& InCatalog, FAutomationTarget InTarget,
	                  std::unique_ptr<ITransportListener> InListener, const IAutomationAccessPolicy& InAccess);
	~FAutomationServer();
	void Poll();
	void StopAdmission();
	std::size_t PendingCount() const;
	const FAutomationTarget& Target() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

// No filesystem/process assumptions: discovery and transports are injected independently.
class FConnectionManager
{
public:
	FConnectionManager(FTransportRegistry& InTransports, ITargetDiscovery& InDiscovery,
	                   const IAutomationAccessPolicy& InAccess);
	~FConnectionManager();
	FArchiveNode List();
	FEndpointRequest Connect(const FArchiveNode& InParameters);
	FEndpointRequest Probe(const FArchiveNode& InParameters);
	FArchiveNode Disconnect(const std::string& InConnection);
	FEndpointRequest Request(const std::string& InConnection, std::string_view InMethod,
	                         const FArchiveNode& InParameters);
	void Poll();
	void Close();

private:
	FEndpointRequest BeginConnection(const FArchiveNode& InParameters, bool bInProbe, std::uint32_t InTimeoutMs);
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

class FAutomationRouter final : public IAutomationEndpoint
{
public:
	FAutomationRouter(IAutomationEndpoint& InLocal, FConnectionManager& InConnections);
	void SetDefaultConnection(std::string InConnection);
	FEndpointRequest Begin(std::string_view InMethod, const FArchiveNode& InParameters) override;
	FArchiveNode Tools() const override;

private:
	IAutomationEndpoint& Local;
	FConnectionManager& Connections;
	std::string DefaultConnection;
};
} // namespace Hyperion
