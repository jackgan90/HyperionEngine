#include "Hyperion/SceneEditing/SceneDocumentHost.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FSceneOpenRequest>()
{
	static const auto Type = MakeRecord<FSceneOpenRequest>(
	    "hyperion.scene.open.request",
	    {Member("document", &FSceneOpenRequest::Document,
	            {.bRequired = true, .Description = "Current scene document, including an empty document."}),
	     Member("revision", &FSceneOpenRequest::Revision,
	            {.bRequired = true, .Description = "Current revision from scene.info."}),
	     Member("path", &FSceneOpenRequest::Path,
	            {.bRequired = true,
	             .Description = "Target-side native scene path. Empty creates an empty scene document."}),
	     Member(
	         "discard", &FSceneOpenRequest::bDiscard,
	         {.Description =
	              "Explicitly discard unsaved scene changes; otherwise save first. Does not close asset documents."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneHostStatus>()
{
	static const auto Type =
	    MakeRecord<FSceneHostStatus>("hyperion.scene.host.status", {Member("scene", &FSceneHostStatus::Scene),
	                                                                Member("error", &FSceneHostStatus::Error)});
	return Type;
}
} // namespace Hyperion
