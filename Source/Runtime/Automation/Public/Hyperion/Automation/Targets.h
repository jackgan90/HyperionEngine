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

enum class ETargetListStopReason
{
	Complete = 0,
	ResultLimit = 1,
	ScanLimit = 2
};

struct FTargetDiscoverySnapshot
{
	std::vector<FAutomationTarget> Targets;
	std::uint32_t Examined{};
	std::uint32_t StaleSkipped{};
	std::uint32_t UnknownOwnership{};
	ETargetListStopReason StopReason = ETargetListStopReason::Complete;
};

template<> std::span<const TRecordEnumEntry<ETargetListStopReason>> RecordEnumEntries<ETargetListStopReason>();
template<> const FRecordDescriptor& RecordType<FTargetDiscoverySnapshot>();

class ITargetDiscovery
{
public:
	virtual ~ITargetDiscovery() = default;
	// Advisory, bounded snapshots. A connection handshake establishes actual identity/availability.
	virtual FTargetDiscoverySnapshot List() = 0;
	// Exact lookup must not depend on enumeration budgets.
	virtual std::optional<FAutomationTarget> Find(const std::string& InInstance) = 0;
};

class FLocalTargetDiscovery final : public ITargetDiscovery
{
public:
	explicit FLocalTargetDiscovery(std::filesystem::path InDirectory = {});
	FTargetDiscoverySnapshot List() override;
	std::optional<FAutomationTarget> Find(const std::string& InInstance) override;
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
