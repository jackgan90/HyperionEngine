#include "Hyperion/SceneEditing/SceneDocumentHost.h"

namespace Hyperion
{
template<> std::span<const TRecordEnumEntry<ESceneDirtyAction>> RecordEnumEntries<ESceneDirtyAction>()
{
	static constexpr TRecordEnumEntry<ESceneDirtyAction> Values[] = {
	    {ESceneDirtyAction::RejectDirty, "RejectDirty", "Refuse to replace unsaved scene changes."},
	    {ESceneDirtyAction::Save, "Save", "Save the dirty scene before the requested action."},
	    {ESceneDirtyAction::Discard, "Discard", "Explicitly discard only the current scene's unsaved changes."}};
	return Values;
}

template<> const FRecordDescriptor& RecordType<FSceneLifecycleRequest>()
{
	static const auto Type = MakeRecord<FSceneLifecycleRequest>(
	    "hyperion.scene.lifecycle.request",
	    {Member("document", &FSceneLifecycleRequest::Document,
	            {.bRequired = true, .Description = "Current document from scene.info, including the closed state."}),
	     Member("revision", &FSceneLifecycleRequest::Revision,
	            {.bRequired = true, .Description = "Current scene revision; zero when no scene is loaded."}),
	     Member("action", &FSceneLifecycleRequest::Action,
	            {.Description = "0: reject dirty (default); 1: save before the action; 2: explicitly discard."}),
	     Member("scenePath", &FSceneLifecycleRequest::ScenePath,
	            {.Description = "Save destination for a dirty untitled scene; otherwise the current path is used."})});
	return Type;
}

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
