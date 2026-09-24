#include "ContentRootOperations.h"
#include "Hyperion/Content/ContentQueries.h"

namespace Hyperion
{
void RegisterContentQueries(FOperationCatalog& InCatalog, FAssetService* InAssets, FContentRootService* InRoots,
                            std::string InOwner)
{
	FOperationInfo Info;
	Info.Id = "content.assets.list";
	Info.Summary = "Search the target native asset index";
	Info.Description = "Bounded path/identity search with optional type filter. Includes Engine and selected Game "
	                   "assets; returns reusable typed asset references. Ordering is by path and identity. Restart "
	                   "pagination after content writes or root changes.";
	Info.Owner = InOwner;
	Info.bReadOnly = true;
	Info.Effects = "Reads the current asset index; does not load asset data.";
	Info.Completion = "Main snapshot page.";
	Info.Keywords = {"content", "browse", "search", "asset", "scene", "material", "texture"};
	Info.Unavailable = InAssets && InRoots ? "" : "Asset/content provider unavailable.";
	const FContentAssetQuery Example{};
	Info.Example = WriteRecordWire(RecordType<FContentAssetQuery>(), &Example);
	InCatalog.Register(MakeOperation<FContentAssetQuery, FContentAssetPage>(
	    Info,
	    [InAssets, InRoots](const FContentAssetQuery& InRequest)
	    {
		    try
		    {
			    return QueryContentAssets(*InAssets, *InRoots, InRequest);
		    }
		    catch (const FContentRootError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
	Info.Id = "content.directory.list";
	Info.Summary = "Discover mounted directories and files, including unindexed native assets";
	Info.Description = "List direct children of /Game or /Engine using the same mounted filesystem as Content Browser. "
	                   "Includes empty directories, non-asset files, native_unindexed candidates and access errors. "
	                   "Unindexed does not imply corrupt; asset.open diagnoses known native paths. Limit 1-100, stable "
	                   "path ordering; not a snapshot, restart after content/root changes.";
	Info.Effects = "Enumerates one mounted directory; no writes or full asset loads.";
	Info.Keywords = {"content", "discover", "directory", "file", "broken", "invalid", "error", "browse"};
	const FContentDirectoryQuery DirectoryExample{};
	Info.Example = WriteRecordWire(RecordType<FContentDirectoryQuery>(), &DirectoryExample);
	InCatalog.Register(MakeOperation<FContentDirectoryQuery, FContentDirectoryPage>(
	    Info,
	    [InAssets, InRoots](const auto& InRequest)
	    {
		    try
		    {
			    return QueryContentDirectory(*InAssets, *InRoots, InRequest);
		    }
		    catch (const FContentRootError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
}

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
	                   "Paths and state belong to the executing target. An attached host persists its own root "
	                   "preferences through its registered content participant.";
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
