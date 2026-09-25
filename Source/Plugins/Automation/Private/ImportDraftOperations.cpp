#include "ImportOperations.h"

namespace Hyperion
{
namespace
{
template<class TRequest, class TResult, class TFunction>
void RegisterDraftOperation(FOperationCatalog& InCatalog, FAssetImportWorkspace* InProvider, const char* InId,
                            const char* InDescription, TRequest InExample, TFunction InFunction,
                            bool bInReadOnly = false)
{
	FOperationInfo Info;
	Info.Id = InId;
	Info.Owner = "automation-assets";
	Info.Summary = InDescription;
	Info.Description = std::string(InDescription) +
	                   " Draft IDs are application-scoped and separate from import tasks. Mutations require the latest "
	                   "draft generation. "
	                   "Property pages contain at most 64 nodes/primitives/parameters/products. Closing a GUI window "
	                   "does not discard a draft.";
	Info.Effects = bInReadOnly ? "Reads shared GUI/agent draft state."
	                           : "Mutates shared import draft state. Only submit publishes files.";
	Info.Completion =
	    "Main snapshot returned; preparing/publishing states are polled through draft.get and import.task.";
	Info.Example = WriteRecordWire(RecordType<TRequest>(), &InExample);
	Info.bReadOnly = bInReadOnly;
	Info.Keywords = {"asset", "import", "draft", "preview"};
	if (!InProvider)
	{
		Info.Unavailable = "Import workspace unavailable; enable assets";
	}
	InCatalog.Register(MakeOperation<TRequest, TResult>(Info,
	                                                    [InProvider, InFunction](const TRequest& InRequest)
	                                                    {
		                                                    try
		                                                    {
			                                                    return InFunction(*InProvider, InRequest);
		                                                    }
		                                                    catch (const FAssetImportError& Failure)
		                                                    {
			                                                    throw FAutomationError(Failure.Code, Failure.what());
		                                                    }
	                                                    }));
}
} // namespace

void RegisterImportDraftOperations(FOperationCatalog& InCatalog, FAssetImportWorkspace* InProvider)
{
	RegisterDraftOperation<FImportRequest, FImportDraftInfo>(
	    InCatalog, InProvider, "asset.import.draft.prepare",
	    "Prepare unpublished converted properties and dependencies; no file publication.",
	    {1, "source.gltf", "/Game/Model.hasset"},
	    [](auto& InWorkspace, const auto& InRequest)
	    {
		    return InWorkspace.PrepareDraft(InRequest);
	    });
	RegisterDraftOperation<FContentRootQuery, FImportDraftList>(
	    InCatalog, InProvider, "asset.import.drafts", "List retained GUI and agent import draft IDs (maximum four).",
	    {},
	    [](auto& InWorkspace, const auto&)
	    {
		    return InWorkspace.DraftList();
	    },
	    true);
	RegisterDraftOperation<FImportDraftQuery, FImportDraftInfo>(
	    InCatalog, InProvider, "asset.import.draft.get",
	    "Inspect draft status and paged converted root properties without bulk payloads.", {"draft-id"},
	    [](auto& InWorkspace, const auto& InRequest)
	    {
		    return InWorkspace.Draft(InRequest);
	    },
	    true);
	RegisterDraftOperation<FImportDraftEdit, FImportDraftInfo>(
	    InCatalog, InProvider, "asset.import.draft.edit",
	    "Replace the complete property override set; preserve other overrides from draft.get. Stable IDs select model "
	    "elements.",
	    {"draft-id", 2},
	    [](auto& InWorkspace, const auto& InRequest)
	    {
		    return InWorkspace.EditDraft(InRequest);
	    });
	RegisterDraftOperation<FImportDraftHistory, FImportDraftInfo>(
	    InCatalog, InProvider, "asset.import.draft.history",
	    "Apply undo, redo or reset to draft properties; never writes files.", {"draft-id", 3, "undo"},
	    [](auto& InWorkspace, const auto& InRequest)
	    {
		    return InWorkspace.DraftHistory(InRequest);
	    });
	RegisterDraftOperation<FImportDraftMutation, FImportTaskInfo>(
	    InCatalog, InProvider, "asset.import.draft.submit",
	    "Publish the confirmed snapshot; returns an import task ID. Changed source fingerprints reject publication.",
	    {"draft-id", 4},
	    [](auto& InWorkspace, const auto& InRequest)
	    {
		    return InWorkspace.SubmitDraft(InRequest);
	    });
	RegisterDraftOperation<FImportDraftDiscard, FImportDraftInfo>(
	    InCatalog, InProvider, "asset.import.draft.discard",
	    "Release an idle draft; modified properties require discard=true. Preparing/publication work must finish "
	    "first.",
	    {"draft-id", 5, true},
	    [](auto& InWorkspace, const auto& InRequest)
	    {
		    return InWorkspace.DiscardDraft(InRequest);
	    });
}
} // namespace Hyperion
