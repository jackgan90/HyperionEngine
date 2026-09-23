#include "Hyperion/Automation/Targets.h"

namespace Hyperion
{
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
	     Member("address", &FAutomationTarget::Address, {.bRequired = true})});
	return Type;
}
} // namespace Hyperion
