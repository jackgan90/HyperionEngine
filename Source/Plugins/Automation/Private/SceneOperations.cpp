#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
template<class TFunction> auto SceneCall(TFunction InFunction)
{
	try
	{
		return InFunction();
	}
	catch (const FSceneEditError& Error)
	{
		throw FAutomationError(Error.Code, Error.what());
	}
}

FOperationInfo SceneInfo(std::string InId, std::string InSummary, std::string InDescription, bool bInReadOnly,
                         const FSceneEditDocument* InDocument)
{
	FOperationInfo Info;
	Info.Id = std::move(InId);
	Info.Summary = std::move(InSummary);
	Info.Description = std::move(InDescription);
	Info.Owner = "automation-scene";
	Info.Effects =
	    bInReadOnly ? "Reads live scene state." : "Changes the live document through the host's shared scene service.";
	Info.Completion = "Main state updated; renderer publication occurs on a subsequent frame. No disk write unless "
	                  "explicitly saving.";
	Info.bReadOnly = bInReadOnly;
	Info.Keywords = {"scene", "live", "transform", "document"};
	Info.Unavailable = InDocument ? "" : "This target has no scene document provider.";
	return Info;
}

template<class TRequest, class TResult, class TFunction>
void AddSceneOperation(FOperationCatalog& InCatalog, FOperationInfo InInfo, const TRequest& InExample,
                       TFunction InFunction)
{
	InInfo.Example = WriteRecordWire(RecordType<TRequest>(), &InExample);
	InCatalog.Register(MakeOperation<TRequest, TResult>(std::move(InInfo),
	                                                    [InFunction](const TRequest& InRequest)
	                                                    {
		                                                    return SceneCall(
		                                                        [&]
		                                                        {
			                                                        return InFunction(InRequest);
		                                                        });
	                                                    }));
}

void RegisterSceneHistory(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	for (const bool bUndo : {true, false})
	{
		auto Info = SceneInfo(bUndo ? "scene.undo" : "scene.redo",
		                      bUndo ? "Undo a live scene transaction" : "Redo a live scene transaction",
		                      "Uses the same history and selection restoration as the GUI. Requires current "
		                      "document/revision and idle interaction state. Empty history is a no-op.",
		                      false, InDocument);
		if (InDocument && !InDocument->HasHistory())
		{
			Info.Unavailable = "This host does not provide scene undo/redo history.";
		}
		AddSceneOperation<FSceneMutationRequest, FSceneDocumentInfo>(
		    InCatalog, std::move(Info), {"document-from-scene.info", 1},
		    [InDocument, bUndo](const FSceneMutationRequest& InRequest)
		    {
			    InDocument->RequireIdle(InRequest.Document, InRequest.Revision);
			    if (bUndo)
			    {
				    InDocument->Undo();
			    }
			    else
			    {
				    InDocument->Redo();
			    }
			    return DescribeSceneDocument(*InDocument);
		    });
	}
}

void RegisterSceneSave(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	auto Info = SceneInfo(
	    "scene.save", "Save a captured live scene revision",
	    "Explicit target-side .hasset destination. Snapshot at admission, shared save point and error reporting. Later "
	    "edits remain dirty. Disconnect does not cancel admitted IO; completion returns current document state.",
	    false, InDocument);
	Info.Effects = "Writes the scene through the host asset service and updates the shared save point; referenced "
	               "assets retain their identities.";
	Info.Completion = "Captured scene persisted. Does not imply GPU completion. Cannot cancel after admission.";
	const FSceneSaveRequest Example{"document-from-scene.info", 1, "/Game/Scenes/Edited.hasset"};
	Info.Example = WriteRecordWire(RecordType<FSceneSaveRequest>(), &Example);
	InCatalog.Register(MakeAsyncOperation<FSceneSaveRequest, FSceneDocumentInfo>(
	    std::move(Info),
	    [InDocument](const FSceneSaveRequest& InRequest)
	    {
		    return SceneCall(
		        [&]() -> TPendingOperation<FSceneDocumentInfo>
		        {
			        InDocument->RequireIdle(InRequest.Document, InRequest.Revision);
			        auto Save = InDocument->Save(InRequest.Path);
			        return {[InDocument, Save]() -> std::optional<FSceneDocumentInfo>
			                {
				                if (!Save.Ready())
				                {
					                return {};
				                }
				                InDocument->PollSave();
				                try
				                {
					                if (!*Save.GetReady())
					                {
						                throw std::runtime_error("Scene save was not committed");
					                }
				                }
				                catch (const std::exception& Error)
				                {
					                throw FAutomationError("save_failed", Error.what());
				                }
				                return DescribeSceneDocument(*InDocument);
			                }};
		        });
	    }));
}
} // namespace

void RegisterSceneOperations(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	AddSceneOperation<FSceneInfoRequest, FSceneDocumentInfo>(
	    InCatalog,
	    SceneInfo("scene.info", "Inspect the live scene document",
	              "Query document identity, revision, loading, busy, dirty, save and history state before editing. The "
	              "same document is shared by all connections and the GUI.",
	              true, InDocument),
	    {},
	    [InDocument](const auto&)
	    {
		    return DescribeSceneDocument(*InDocument);
	    });
	AddSceneOperation<FSceneListRequest, FSceneNodePage>(
	    InCatalog,
	    SceneInfo("scene.nodes.list", "List live scene nodes",
	              "Bounded snapshot pages (1-100 nodes). Supply the current document and revision; restart pagination "
	              "after edits. Handles are scoped to this target and document.",
	              true, InDocument),
	    {"document-from-scene.info", 1, 0, 50},
	    [InDocument](const auto& InRequest)
	    {
		    return ListSceneNodes(*InDocument, InRequest);
	    });
	AddSceneOperation<FSceneNodeRequest, FSceneNodeInfo>(
	    InCatalog,
	    SceneInfo("scene.node.get", "Inspect a live scene node",
	              "Returns authored local and derived world affine matrices with name, kind and parent. Reads do not "
	              "change GUI selection.",
	              true, InDocument),
	    {"document-from-scene.info", {1, 0, 1}},
	    [InDocument](const auto& InRequest)
	    {
		    return DescribeSceneNode(*InDocument, InRequest);
	    });
	AddSceneOperation<FSceneTransformRequest, FSceneDocumentInfo>(
	    InCatalog,
	    SceneInfo("scene.nodes.set_transform", "Set live node transforms atomically",
	              "Supply current document and revision, plus 1-128 distinct handles and column-major local affine "
	              "matrices. Rejects stale targets, non-finite/invalid transforms, active GUI gestures and modals. "
	              "Creates one Editor history entry and leaves selection unchanged. Save explicitly.",
	              false, InDocument),
	    {"document-from-scene.info", 1, {{{1, 0, 1}, Identity()}}},
	    [InDocument](const auto& InRequest)
	    {
		    return SetSceneTransforms(*InDocument, InRequest);
	    });
	RegisterSceneHistory(InCatalog, InDocument);
	RegisterSceneSave(InCatalog, InDocument);
}
} // namespace Hyperion
