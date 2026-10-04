#include "Hyperion/Automation/Operation.h"
#include "Hyperion/Core/Identity.h"

namespace Hyperion
{
std::string CreateAutomationIdentity()
{
	return CreateEphemeralIdentity();
}

FAutomationError::FAutomationError(FErrorCode InCode, std::string InMessage, std::string InPath, FArchiveNode InDetails)
    : FCodedError(std::move(InCode), std::move(InMessage)), Path(std::move(InPath)), Details(std::move(InDetails))
{
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
		return AutomationFailure(AutomationErrors::InvalidArguments, Error.what(), Error.Path);
	}
	catch (const FCodedError& Error)
	{
		return AutomationFailure(Error.Code, Error.what());
	}
	catch (const std::invalid_argument& Error)
	{
		return AutomationFailure(AutomationErrors::InvalidArguments, Error.what());
	}
	catch (const std::exception& Error)
	{
		return AutomationFailure(AutomationErrors::OperationFailed, Error.what());
	}
	catch (...)
	{
		return AutomationFailure(AutomationErrors::InternalError, "Unknown operation failure");
	}
}
} // namespace Hyperion
