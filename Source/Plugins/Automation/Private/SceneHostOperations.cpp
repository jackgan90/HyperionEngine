#include "Hyperion/SceneEditing/SceneDocumentHost.h"
#include "SceneOperations.h"

namespace Hyperion
{
void RegisterSceneHostOperations(FOperationCatalog& InCatalog, ISceneDocumentHost* InHost)
{
	FOperationInfo Info;
	Info.Id = "scene.open";
	Info.Summary = "Open or clear the target scene document";
	Info.Description = "Uses the host scene transition service. Save dirty scene changes first or explicitly discard "
	                   "them. Empty path opens an empty document. Asset tabs remain open. Old scene handles are "
	                   "invalidated. A load failure is reported without claiming that the old scene was restored.";
	Info.Owner = "automation-scene";
	Info.Effects =
	    "Replaces the active scene and its history, selection and browsing view. Does not save or implicitly discard.";
	Info.Completion = "Target scene resources are ready, or a load failure is reported. GPU presentation is not "
	                  "implied. Once accepted, the load cannot be cancelled through the job.";
	Info.Keywords = {"scene", "open", "close", "new", "document", "load"};
	Info.Unavailable = InHost ? "" : "This host does not publish scene document transitions.";
	const FSceneOpenRequest Example{"document-from-scene.info", 0, "/Game/Scenes/Scene.hasset", false};
	Info.Example = WriteRecordWire(RecordType<FSceneOpenRequest>(), &Example);
	InCatalog.Register(MakeAsyncOperation<FSceneOpenRequest, FSceneHostStatus>(
	    std::move(Info),
	    [InHost](const FSceneOpenRequest& InRequest) -> TPendingOperation<FSceneHostStatus>
	    {
		    try
		    {
			    InHost->OpenDocument(InRequest);
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
		    const auto Document = InHost->DocumentStatus().Scene.Document;
		    return {[InHost, Document]() -> std::optional<FSceneHostStatus>
		            {
			            auto Status = InHost->PollDocument();
			            if (Status.Scene.Document != Document)
			            {
				            throw FAutomationError("stale_document", "Another transition replaced the requested scene");
			            }
			            if (!Status.Error.empty())
			            {
				            throw FAutomationError("load_failed", Status.Error);
			            }
			            return Status.Scene.bReady ? std::optional(std::move(Status)) : std::nullopt;
		            }};
	    }));
	FOperationInfo Status;
	Status.Id = "scene.status";
	Status.Summary = "Inspect scene loading and failure state";
	Status.Description = "Read the actual target document, readiness and load error without waiting.";
	Status.Owner = "automation-scene";
	Status.bReadOnly = true;
	Status.Effects = "No mutation.";
	Status.Completion = "Current Main snapshot.";
	Status.Unavailable = InHost ? "" : "This host does not publish scene document transitions.";
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FSceneHostStatus>(std::move(Status),
	                                                                      [InHost](const auto&)
	                                                                      {
		                                                                      return InHost->DocumentStatus();
	                                                                      }));
}
} // namespace Hyperion
