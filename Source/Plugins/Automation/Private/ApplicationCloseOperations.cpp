#include "Hyperion/Config/ApplicationClose.h"
#include "SceneOperations.h"

namespace Hyperion
{
void RegisterApplicationClose(FOperationCatalog& InCatalog, IApplicationClose* InHost)
{
	FOperationInfo Info;
	Info.Id = "application.close.status";
	Info.Owner = "automation-scene";
	Info.Summary = "Read application close and unsaved-work state";
	Info.Description = "Normal host exit shares GUI dirty decisions and persistence. A saving request continues after "
	                   "disconnect. Query failure state while connected; successful exit withdraws the target.";
	Info.bReadOnly = true;
	Info.Effects = "Reads the host close state.";
	Info.Completion = "Main snapshot.";
	Info.Unavailable = InHost ? "" : "No application close provider on this target.";
	InCatalog.Register(
	    MakeOperation<FSceneInfoRequest, FApplicationCloseState>(Info,
	                                                             [InHost](const auto&)
	                                                             {
		                                                             return InHost->ApplicationCloseState();
	                                                             }));
	Info.Id = "application.close.request";
	Info.Summary = "Request normal application exit or cancel pending exit";
	Info.bReadOnly = false;
	Info.Effects = "May save documents or explicitly discard unsaved work, then request normal application shutdown.";
	Info.Completion = "Decision accepted only. Saving is asynchronous; failure leaves the application open. Closing "
	                  "disconnects clients after bounded reply draining.";
	const FApplicationCloseRequest Example;
	Info.Example = WriteRecordWire(RecordType<FApplicationCloseRequest>(), &Example);
	InCatalog.Register(MakeOperation<FApplicationCloseRequest, FApplicationCloseState>(
	    Info,
	    [InHost](const auto& InRequest)
	    {
		    try
		    {
			    return InHost->RequestApplicationClose(InRequest);
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
}
} // namespace Hyperion
