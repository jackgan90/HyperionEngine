#include "Hyperion/SceneEditing/SceneDocumentHost.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
void RegisterSceneLifecycle(FOperationCatalog& InCatalog, ISceneDocumentHost* InHost, ESceneDocumentAction InAction,
                            const char* InId, const char* InSummary)
{
	FOperationInfo Info;
	Info.Id = InId;
	Info.Summary = InSummary;
	Info.Description = "Uses the same scene lifecycle service as Editor. Dirty scenes require explicit save or "
	                   "discard. Supply scenePath to save an untitled scene. Asset tabs and the content root remain "
	                   "available. Old scene documents and handles are invalidated.";
	Info.Owner = "automation-scene";
	Info.Effects = "Replaces or closes the current scene, history, selection and transient view state. Save is "
	               "explicit; default requests reject dirty scenes.";
	Info.Completion = "The new empty scene is ready, or the current scene is closed. Accepted save/retirement work "
	                  "is not cancellable through the job; failure retains the original scene before retirement.";
	Info.Keywords = {"scene", "new", "close", "document", "save", "empty"};
	Info.Unavailable = InHost ? "" : "This host does not publish scene document transitions.";
	const FSceneLifecycleRequest Example{"document-from-scene.info", 0};
	Info.Example = WriteRecordWire(RecordType<FSceneLifecycleRequest>(), &Example);
	InCatalog.Register(MakeAsyncOperation<FSceneLifecycleRequest, FSceneHostStatus>(
	    std::move(Info),
	    [InHost, InAction](const FSceneLifecycleRequest& InRequest) -> TPendingOperation<FSceneHostStatus>
	    {
		    const auto Pending = InHost->ChangeDocument(InAction, InRequest);
		    return {[InHost, Pending]() -> std::optional<FSceneHostStatus>
		            {
			            if (!Pending->Result && !Pending->Failure)
			            {
				            InHost->PollDocument();
			            }
			            if (Pending->Failure)
			            {
				            std::rethrow_exception(Pending->Failure);
			            }
			            return Pending->Result;
		            }};
	    }));
}
} // namespace

void RegisterSceneHostOperations(FOperationCatalog& InCatalog, ISceneDocumentHost* InHost)
{
	RegisterSceneLifecycle(InCatalog, InHost, ESceneDocumentAction::New, "scene.new", "Create an empty untitled scene");
	RegisterSceneLifecycle(InCatalog, InHost, ESceneDocumentAction::Close, "scene.close", "Close the active scene");
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
		    InHost->OpenDocument(InRequest);

		    const auto Document = InHost->DocumentStatus().Scene.Document;
		    return {[InHost, Document]() -> std::optional<FSceneHostStatus>
		            {
			            auto Status = InHost->PollDocument();
			            if (Status.Scene.Document != Document)
			            {
				            throw FAutomationError(AutomationErrors::StaleDocument,
				                                   "Another transition replaced the requested scene");
			            }
			            if (!Status.Error.empty())
			            {
				            throw FAutomationError(AutomationErrors::LoadFailed, Status.Error);
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
