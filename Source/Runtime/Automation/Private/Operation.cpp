#include "Hyperion/Automation/Operation.h"
#include "Hyperion/Core/Identity.h"

namespace Hyperion
{
std::string CreateAutomationIdentity()
{
	return CreateEphemeralIdentity();
}

FAutomationError::FAutomationError(std::string InCode, std::string InMessage, std::string InPath,
                                   FArchiveNode InDetails)
    : std::runtime_error(std::move(InMessage)), Code(std::move(InCode)), Path(std::move(InPath)),
      Details(std::move(InDetails))
{
}

FArchiveNode AutomationFailure(std::string InCode, std::string InMessage, std::string InPath, FArchiveNode InDetails)
{
	return FArchiveNode(
	    FArchiveNode::FObject{{"status", WriteValue(std::string("failed"))},
	                          {"error", FArchiveNode(FArchiveNode::FObject{{"code", WriteValue(InCode)},
	                                                                       {"message", WriteValue(InMessage)},
	                                                                       {"path", WriteValue(InPath)},
	                                                                       {"details", std::move(InDetails)}})}});
}

FArchiveNode CurrentAutomationFailure()
{
	try
	{
		throw;
	}
	catch (const FAutomationError& Error)
	{
		return AutomationFailure(Error.Code, Error.what(), Error.Path, Error.Details);
	}
	catch (const FWireError& Error)
	{
		return AutomationFailure("invalid_arguments", Error.what(), Error.Path);
	}
	catch (const std::invalid_argument& Error)
	{
		return AutomationFailure("invalid_arguments", Error.what());
	}
	catch (const std::exception& Error)
	{
		return AutomationFailure("operation_failed", Error.what());
	}
	catch (...)
	{
		return AutomationFailure("internal_error", "Unknown operation failure");
	}
}
} // namespace Hyperion
