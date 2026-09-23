#include "ContentRootOperations.h"

namespace Hyperion
{
namespace
{
template<class TRequest, class TFunction>
void RegisterRoot(FOperationCatalog& InCatalog, FContentRootService* InRoots, const std::string& InOwner,
                  std::string InId, std::string InSummary, const TRequest& InExample, bool bInReadOnly,
                  TFunction InFunction)
{
	FOperationInfo Info;
	Info.Id = std::move(InId);
	Info.Summary = std::move(InSummary);
	Info.Description = "Query or change the current session's /Game asset directory. Engine content stays mounted. "
	                   "Query the generation before changing roots. Invalid, dirty, busy or stale requests preserve "
	                   "existing content. Setting the same canonical directory and permissions preserves documents. "
	                   "Pending edits/saves must finish first; save dirty documents or explicitly discard them. "
	                   "Root state is process-local and does not update Editor preferences in another process.";
	Info.Owner = InOwner;
	Info.Effects =
	    bInReadOnly
	        ? "No mutation."
	        : "Closes all old content documents and invalidates their handles; changes Game mapping and asset index.";
	Info.Completion =
	    "Synchronous on Main; candidate discovery and all content participants complete before returning.";
	Info.Example = WriteRecordWire(RecordType<TRequest>(), &InExample);
	Info.bReadOnly = bInReadOnly;
	Info.Keywords = {"asset", "root", "directory", "project", "content"};
	Info.Unavailable =
	    InRoots ? "" : "Content service is unavailable; enable assets with a valid Engine content directory";
	InCatalog.Register(MakeOperation<TRequest, FContentRootInfo>(
	    std::move(Info),
	    [InRoots, Function = std::move(InFunction)](const TRequest& InRequest)
	    {
		    try
		    {
			    return Function(*InRoots, InRequest);
		    }
		    catch (const FContentRootError& Failure)
		    {
			    throw FAutomationError(Failure.Code, Failure.what());
		    }
		    catch (const std::exception& Failure)
		    {
			    throw FAutomationError("invalid_root", Failure.what(), "directory");
		    }
	    }));
}
} // namespace

void RegisterContentRootOperations(FOperationCatalog& InCatalog, FContentRootService* InRoots, std::string InOwner,
                                   bool bInMutable)
{
	RegisterRoot(InCatalog, InRoots, InOwner, "content.root.get", "Query the current asset root", FContentRootQuery{},
	             true,
	             [](FContentRootService& InService, const FContentRootQuery&)
	             {
		             return InService.Info();
	             });
	if (!bInMutable)
	{
		return;
	}
	RegisterRoot(InCatalog, InRoots, InOwner, "content.root.set", "Select an asset root directory",
	             FContentRootRequest{"Assets", 0}, false,
	             [](FContentRootService& InService, const FContentRootRequest& InRequest)
	             {
		             return InService.Set(InRequest);
	             });
	RegisterRoot(InCatalog, InRoots, InOwner, "content.root.clear", "Clear the current asset root",
	             FContentRootClearRequest{0}, false,
	             [](FContentRootService& InService, const FContentRootClearRequest& InRequest)
	             {
		             return InService.Clear(InRequest);
	             });
}
} // namespace Hyperion
