#include "Hyperion/Automation/Targets.h"

namespace Hyperion
{
template<> std::span<const TRecordEnumEntry<ETargetListStopReason>> RecordEnumEntries<ETargetListStopReason>()
{
	static constexpr TRecordEnumEntry<ETargetListStopReason> Entries[] = {
	    {ETargetListStopReason::Complete, "complete"},
	    {ETargetListStopReason::ResultLimit, "result_limit"},
	    {ETargetListStopReason::ScanLimit, "scan_limit"}};
	return Entries;
}

template<> const FRecordDescriptor& RecordType<FTargetDiscoverySnapshot>()
{
	static const auto Type = MakeRecord<FTargetDiscoverySnapshot>(
	    "automation.targets.snapshot",
	    {Member("targets", &FTargetDiscoverySnapshot::Targets), Member("examined", &FTargetDiscoverySnapshot::Examined),
	     Member("staleSkipped", &FTargetDiscoverySnapshot::StaleSkipped),
	     Member("unknownOwnership", &FTargetDiscoverySnapshot::UnknownOwnership),
	     Member("stopReason", &FTargetDiscoverySnapshot::StopReason)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FTransportAddress>()
{
	static const auto Type = MakeRecord<FTransportAddress>(
	    "automation.transport.address",
	    {Member("scheme", &FTransportAddress::Scheme,
	            {.bRequired = true, .Description = "Registered transport scheme."}),
	     Member("address", &FTransportAddress::Address,
	            {.bRequired = true, .Description = "Provider-interpreted UTF-8 address; not a domain asset path."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAutomationTarget>()
{
	static const auto Type = MakeRecord<FAutomationTarget>(
	    "automation.target",
	    {Member("instance", &FAutomationTarget::Instance,
	            {.bRequired = true, .Description = "Boot-specific instance identity, verified during handshake."}),
	     Member("application", &FAutomationTarget::Application, {.bRequired = true}),
	     Member("build", &FAutomationTarget::Build, {.bRequired = true}),
	     Member("address", &FAutomationTarget::Address, {.bRequired = true}),
	     Member("mode", &FAutomationTarget::Mode,
	            {.Description = "Startup host mode, e.g. Editor. Empty on older hosts."}),
	     Member("label", &FAutomationTarget::Label,
	            {.Description =
	                 "Human-readable startup label; advisory and not an identity or current document snapshot."})});
	return Type;
}
} // namespace Hyperion
