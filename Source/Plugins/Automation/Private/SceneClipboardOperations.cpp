#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
template<class TRequest, class TResult, class TFunction>
void AddClipboardOperation(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument, const char* InId,
                           const char* InSummary, const char* InDescription, const char* InEffects, bool bInReadOnly,
                           TRequest InExample, TFunction InFunction)
{
	FOperationInfo Info;
	Info.Id = InId;
	Info.Summary = InSummary;
	Info.Description = InDescription;
	Info.Owner = "automation-scene";
	Info.Keywords = {"scene", "clipboard", "copy", "paste", "selection"};
	Info.bReadOnly = bInReadOnly;
	Info.Effects = InEffects;
	Info.Completion = "Completed on Main. Scene rendering observes committed edits on subsequent frames.";
	Info.Unavailable = InDocument && InDocument->HasClipboardProvider() ? "" : "No scene clipboard provider.";
	Info.Example = WriteRecordWire(RecordType<TRequest>(), &InExample);
	InCatalog.Register(MakeOperation<TRequest, TResult>(std::move(Info),
	                                                    [InDocument, InFunction](const TRequest& InRequest)
	                                                    {
		                                                    try
		                                                    {
			                                                    return InFunction(*InDocument, InRequest);
		                                                    }
		                                                    catch (const FSceneEditError& Error)
		                                                    {
			                                                    throw FAutomationError(Error.Code, Error.what());
		                                                    }
	                                                    }));
}
} // namespace

void RegisterSceneClipboard(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	AddClipboardOperation<FSceneMutationRequest, FSceneClipboardInfo>(
	    InCatalog, InDocument, "scene.selection.copy", "Copy selected scene subtrees",
	    "Captures immutable selected subtrees, deduplicating descendants. Same Editor and document only. "
	    "Requires idle document/revision. Maximum 16384 nodes and 64 MiB authored data. Empty selection is a no-op.",
	    "Replaces the system clipboard with an object token and name text. Does not change scene or history.", false,
	    {"document-from-scene.info", 1},
	    [](auto& InDocument, const auto& InRequest)
	    {
		    return InDocument.CopySelection(InRequest.Document, InRequest.Revision);
	    });
	AddClipboardOperation<FSceneInfoRequest, FSceneClipboardInfo>(
	    InCatalog, InDocument, "scene.clipboard.info", "Inspect scene clipboard availability",
	    "Reads the current system token and validates document scope, external references and interaction state. "
	    "Ordinary text replacement disables object paste. Does not expose clipboard text or snapshot contents.",
	    "Reads clipboard availability without changing the system clipboard, scene or history.", true, {},
	    [](auto& InDocument, const auto&)
	    {
		    return InDocument.ClipboardInfo();
	    });
	AddClipboardOperation<FSceneMutationRequest, FSceneDocumentInfo>(
	    InCatalog, InDocument, "scene.clipboard.paste", "Paste captured scene subtrees",
	    "Pastes the current document's clipboard with fresh IDs and numbered names under original parents, "
	    "preserving local matrices and component values. One atomic history entry; selects mapped explicit selection. "
	    "Missing parents or unsupported data fail the whole batch. Query scene.selection.get for created handles.",
	    "Creates scene objects, changes selection and history, and marks the document dirty. No implicit save.", false,
	    {"document-from-scene.info", 1},
	    [](auto& InDocument, const auto& InRequest)
	    {
		    InDocument.PasteClipboard(InRequest.Document, InRequest.Revision);
		    return DescribeSceneDocument(InDocument);
	    });
}
} // namespace Hyperion
