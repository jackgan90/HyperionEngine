#include "Hyperion/Renderer/SceneViewport.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
struct FViewportEdit
{
	std::string Document;
	std::uint64_t Revision{};
	std::optional<FSceneCameraView> Camera;
	std::optional<float> Speed;
	FSceneViewportOptions Options;
};

struct FPreviewCameraRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::optional<FSceneHandle> Camera;
};
} // namespace

template<> const FRecordDescriptor& RecordType<FViewportEdit>()
{
	static const auto Type = MakeRecord<FViewportEdit>(
	    "automation.viewport.edit",
	    {Member("document", &FViewportEdit::Document, {.bRequired = true}),
	     Member("revision", &FViewportEdit::Revision, {.bRequired = true}), Member("camera", &FViewportEdit::Camera),
	     Member("speed", &FViewportEdit::Speed), Member("options", &FViewportEdit::Options)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FPreviewCameraRequest>()
{
	static const auto Type = MakeRecord<FPreviewCameraRequest>(
	    "automation.viewport.preview",
	    {Member("document", &FPreviewCameraRequest::Document, {.bRequired = true}),
	     Member("revision", &FPreviewCameraRequest::Revision, {.bRequired = true}),
	     Member("camera", &FPreviewCameraRequest::Camera,
	            {.bRequired = true, .Description = "Scene camera handle, or null to return to browsing view."})});
	return Type;
}

namespace
{
template<class TRequest, class TFunction>
void AddView(FOperationCatalog& InCatalog, ISceneViewport* InView, FSceneEditDocument* InDocument, std::string InId,
             std::string InDescription, bool bInAuthoring, TFunction InFunction, bool bInCameraControls = false)
{
	FOperationInfo Info;
	Info.Id = std::move(InId);
	Info.Owner = "automation-scene";
	Info.Summary = Info.Id;
	Info.Description = std::move(InDescription);
	Info.Effects = bInAuthoring ? "Shared scene history transaction; save explicitly."
	                            : "Changes temporary viewport state, without scene history or disk writes.";
	Info.Completion = "State updated on Main; subsequent rendered frames observe the change.";
	const TRequest Example{"document-from-scene.info", 1};
	Info.Example = WriteRecordWire(RecordType<TRequest>(), &Example);
	Info.Unavailable = InView && (!bInAuthoring || InDocument) ? "" : "Viewport provider unavailable.";
	Info.Description += " Hosts without a scene document (ModelViewer) use document=\"\" and revision=\"0\" for "
	                    "temporary view operations.";
	if ((bInAuthoring || bInCameraControls) && InView && !InView->ViewportState().bCameraAuthoring)
	{
		Info.Unavailable = "This viewport has no camera authoring controls.";
	}
	InCatalog.Register(MakeOperation<TRequest, FSceneViewportState>(
	    Info,
	    [InView, InDocument, InFunction, bInAuthoring](const TRequest& InRequest)
	    {
		    try
		    {
			    if (bInAuthoring)
			    {
				    InDocument->RequireIdle(InRequest.Document, InRequest.Revision);
			    }
			    else if (InDocument)
			    {
				    ListSceneNodes(*InDocument, {InRequest.Document, InRequest.Revision, 0, 1});
			    }
			    else if (!InRequest.Document.empty() || InRequest.Revision != 0)
			    {
				    throw FSceneEditError("stale_document", "This viewport requires an empty document and revision 0");
			    }
			    InFunction(*InView, InRequest);
			    return InView->ViewportState();
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
}
} // namespace

void RegisterViewportOperations(FOperationCatalog& InCatalog, ISceneViewport* InView, FSceneEditDocument* InDocument)
{
	FOperationInfo Get;
	Get.Id = "view.get";
	Get.Owner = "automation-scene";
	Get.Summary = "Read target viewport camera and supported controls";
	Get.Description = "Camera is the browsing camera; previewCamera identifies an optional scene camera source. Null "
	                  "option fields mean unsupported controls.";
	Get.bReadOnly = true;
	Get.Effects = "Reads temporary viewport state.";
	Get.Completion = "Current Main snapshot.";
	Get.Unavailable = InView ? "" : "Viewport provider unavailable.";
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FSceneViewportState>(Get,
	                                                                         [InView](const auto&)
	                                                                         {
		                                                                         return InView->ViewportState();
	                                                                         }));
	AddView<FViewportEdit>(
	    InCatalog, InView, InDocument, "view.set",
	    "Set one of camera, speed or options per request. Camera returns to browsing view. Speed applies to fly "
	    "navigation; options unsupported by this host are rejected. Read view.get first.",
	    false,
	    [InDocument](auto& InView, const auto& InRequest)
	    {
		    if (InRequest.Camera && InRequest.Speed)
		    {
			    throw std::invalid_argument("Set camera and speed in separate requests");
		    }
		    const auto Options = WriteRecordWire(RecordType<FSceneViewportOptions>(), &InRequest.Options);
		    bool bOptions{};
		    for (const auto& [Key, Value] : std::get<FArchiveNode::FObject>(Options.Value))
		    {
			    bOptions |= !std::holds_alternative<std::monostate>(Value.Value);
		    }
		    if (bOptions && (InRequest.Camera || InRequest.Speed))
		    {
			    throw std::invalid_argument("Set options separately from camera/speed");
		    }
		    if (InRequest.Camera)
		    {
			    if (InDocument)
			    {
				    InDocument->RequireIdle(InRequest.Document, InRequest.Revision);
			    }
			    InView.SetViewportCamera(*InRequest.Camera);
		    }
		    else if (InRequest.Speed)
		    {
			    InView.SetViewportSpeed(*InRequest.Speed);
		    }
		    else
		    {
			    InView.SetViewportOptions(InRequest.Options);
		    }
	    });
	AddView<FSceneMutationRequest>(InCatalog, InView, InDocument, "view.frame_scene",
	                               "Fit the browsing camera to scene bounds.", false,
	                               [](auto& InView, const auto&)
	                               {
		                               InView.FrameScene();
	                               });
	AddView<FPreviewCameraRequest>(
	    InCatalog, InView, InDocument, "view.preview_camera",
	    "Select an enabled scene camera or return to the browsing view.", false,
	    [InDocument](auto& InView, const auto& InRequest)
	    {
		    InDocument->RequireIdle(InRequest.Document, InRequest.Revision);
		    InView.PreviewSceneCamera(InRequest.Camera);
	    },
	    true);
	AddView<FSceneMutationRequest>(InCatalog, InView, InDocument, "view.save_initial",
	                               "Save current browsing camera as the scene initial view.", true,
	                               [](auto& InView, const auto&)
	                               {
		                               InView.SaveInitialView();
	                               });
	AddView<FSceneMutationRequest>(InCatalog, InView, InDocument, "view.create_camera",
	                               "Create and select a scene camera from the browsing view.", true,
	                               [](auto& InView, const auto&)
	                               {
		                               InView.CreateViewCamera();
	                               });
	AddView<FPreviewCameraRequest>(InCatalog, InView, InDocument, "view.apply_to_camera",
	                               "Apply browsing view lens and world pose to an existing scene camera.", true,
	                               [](auto& InView, const auto& InRequest)
	                               {
		                               if (!InRequest.Camera)
		                               {
			                               throw std::invalid_argument("Camera handle required");
		                               }
		                               InView.ApplyViewToCamera(*InRequest.Camera);
	                               });
}
} // namespace Hyperion
