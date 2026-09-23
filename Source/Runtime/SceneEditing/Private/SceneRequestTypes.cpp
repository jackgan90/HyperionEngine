#include "Hyperion/SceneEditing/SceneRequests.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FSceneHandle>()
{
	static const auto Type = MakeRecord<FSceneHandle>(
	    "hyperion.scene.handle",
	    {Member("scene", &FSceneHandle::Scene,
	            {.bRequired = true, .Description = "Opaque scene instance integer; retain exactly."}),
	     Member("slot", &FSceneHandle::Slot, {.bRequired = true, .Description = "Opaque slot."}),
	     Member("generation", &FSceneHandle::Generation,
	            {.bRequired = true, .Description = "Opaque generation; rejects replaced nodes."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneInfoRequest>()
{
	static const auto Type = MakeRecord<FSceneInfoRequest>("hyperion.scene.info.request", {});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneMutationRequest>()
{
	static const auto Type = MakeRecord<FSceneMutationRequest>(
	    "hyperion.scene.mutation",
	    {Member("document", &FSceneMutationRequest::Document,
	            {.bRequired = true, .Description = "Current document from scene.info."}),
	     Member("revision", &FSceneMutationRequest::Revision,
	            {.bRequired = true, .Description = "Current decimal-string scene revision."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneListRequest>()
{
	static const auto Type = MakeRecord<FSceneListRequest>(
	    "hyperion.scene.list",
	    {Member("document", &FSceneListRequest::Document,
	            {.bRequired = true, .Description = "Current document from scene.info."}),
	     Member("revision", &FSceneListRequest::Revision,
	            {.bRequired = true, .Description = "Current scene revision; stable across this pagination."}),
	     Member("offset", &FSceneListRequest::Offset), Member("limit", &FSceneListRequest::Limit)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneNodeRequest>()
{
	static const auto Type = MakeRecord<FSceneNodeRequest>(
	    "hyperion.scene.node.request",
	    {Member("document", &FSceneNodeRequest::Document,
	            {.bRequired = true, .Description = "Current document from scene.info."}),
	     Member("handle", &FSceneNodeRequest::Handle,
	            {.bRequired = true, .Description = "Opaque node handle from scene.nodes.list."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneTransformValue>()
{
	static const auto Type = MakeRecord<FSceneTransformValue>(
	    "hyperion.scene.transform",
	    {Member("handle", &FSceneTransformValue::Handle, {.bRequired = true, .Description = "Opaque node handle."}),
	     Member(
	         "local", &FSceneTransformValue::Local,
	         {.bRequired = true,
	          .Description = "Finite local affine matrix; column-major values with translation at indices 12-14."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneTransformRequest>()
{
	static const auto Type = MakeRecord<FSceneTransformRequest>(
	    "hyperion.scene.transforms",
	    {Member("document", &FSceneTransformRequest::Document,
	            {.bRequired = true, .Description = "Current document from scene.info."}),
	     Member(
	         "revision", &FSceneTransformRequest::Revision,
	         {.bRequired = true, .Description = "Current scene revision. Entire batch validates before any mutation."}),
	     Member("transforms", &FSceneTransformRequest::Transforms,
	            {.bRequired = true, .Description = "1-128 distinct node transforms; one history transaction."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneSaveRequest>()
{
	static const auto Type = MakeRecord<FSceneSaveRequest>(
	    "hyperion.scene.save",
	    {Member("document", &FSceneSaveRequest::Document,
	            {.bRequired = true, .Description = "Current document from scene.info."}),
	     Member("revision", &FSceneSaveRequest::Revision,
	            {.bRequired = true, .Description = "Current revision at snapshot admission."}),
	     Member(
	         "path", &FSceneSaveRequest::Path,
	         {.bRequired = true,
	          .Description = "Explicit .hasset destination in the target process filesystem or mounted namespace."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneDocumentInfo>()
{
	static const auto Type = MakeRecord<FSceneDocumentInfo>(
	    "hyperion.scene.info",
	    {Member("document", &FSceneDocumentInfo::Document), Member("path", &FSceneDocumentInfo::Path),
	     Member("revision", &FSceneDocumentInfo::Revision), Member("nodes", &FSceneDocumentInfo::Nodes),
	     Member("loaded", &FSceneDocumentInfo::bLoaded), Member("ready", &FSceneDocumentInfo::bReady),
	     Member("dirty", &FSceneDocumentInfo::bDirty), Member("busy", &FSceneDocumentInfo::bBusy),
	     Member("saving", &FSceneDocumentInfo::bSaving), Member("history", &FSceneDocumentInfo::bHistory),
	     Member("canUndo", &FSceneDocumentInfo::bCanUndo), Member("canRedo", &FSceneDocumentInfo::bCanRedo)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneNodeInfo>()
{
	static const auto Type = MakeRecord<FSceneNodeInfo>(
	    "hyperion.scene.node",
	    {Member("document", &FSceneNodeInfo::Document), Member("revision", &FSceneNodeInfo::Revision),
	     Member("handle", &FSceneNodeInfo::Handle), Member("id", &FSceneNodeInfo::Id),
	     Member("name", &FSceneNodeInfo::Name), Member("kind", &FSceneNodeInfo::Kind),
	     Member("parent", &FSceneNodeInfo::Parent), Member("local", &FSceneNodeInfo::Local),
	     Member("world", &FSceneNodeInfo::World), Member("enabled", &FSceneNodeInfo::bEnabled),
	     Member("effectiveEnabled", &FSceneNodeInfo::bEffectiveEnabled)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneNodePage>()
{
	static const auto Type = MakeRecord<FSceneNodePage>(
	    "hyperion.scene.page", {Member("document", &FSceneNodePage::Document),
	                            Member("revision", &FSceneNodePage::Revision), Member("total", &FSceneNodePage::Total),
	                            Member("next", &FSceneNodePage::Next), Member("nodes", &FSceneNodePage::Nodes)});
	return Type;
}

} // namespace Hyperion
