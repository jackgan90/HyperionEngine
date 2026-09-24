#pragma once
#include "Hyperion/Automation/Operation.h"
#include "Hyperion/Transport/Transport.h"

namespace Hyperion
{
struct FAutomationTarget
{
	std::string Instance;
	std::string Application;
	std::string Build;
	FTransportAddress Address;
	std::string Mode;
	std::string Label;
};

template<> const FRecordDescriptor& RecordType<FTransportAddress>();
template<> const FRecordDescriptor& RecordType<FAutomationTarget>();

class ITargetDiscovery
{
public:
	virtual ~ITargetDiscovery() = default;
	// Advisory, bounded snapshots. A connection handshake establishes actual identity/availability.
	virtual std::vector<FAutomationTarget> List() = 0;
};

class FLocalTargetDiscovery final : public ITargetDiscovery
{
public:
	explicit FLocalTargetDiscovery(std::filesystem::path InDirectory = {});
	std::vector<FAutomationTarget> List() override;
	void Publish(const FAutomationTarget& InTarget);
	void Withdraw(const std::string& InInstance) noexcept;

private:
	std::filesystem::path Directory;
};

class IAutomationAccessPolicy
{
public:
	virtual ~IAutomationAccessPolicy() = default;
	virtual bool Admit(const FTransportPeer& InPeer) const = 0;
};

class FCurrentUserAccessPolicy final : public IAutomationAccessPolicy
{
public:
	bool Admit(const FTransportPeer& InPeer) const override;
};
} // namespace Hyperion
