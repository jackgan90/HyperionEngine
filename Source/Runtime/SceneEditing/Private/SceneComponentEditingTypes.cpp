#include "Hyperion/SceneEditing/SceneComponentEditing.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FSceneComponentRequest>()
{
	static const auto Type = MakeRecord<FSceneComponentRequest>(
	    "hyperion.scene.component.request",
	    {
	        Member("document", &FSceneComponentRequest::Document, {.bRequired = true}),
	        Member("revision", &FSceneComponentRequest::Revision, {.bRequired = true}),
	        Member("handle", &FSceneComponentRequest::Handle, {.bRequired = true}),
	        Member("component", &FSceneComponentRequest::Component, {.bRequired = true}),
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneComponentStructureRequest>()
{
	static const auto Type = MakeRecord<FSceneComponentStructureRequest>(
	    "hyperion.scene.component.structure.request",
	    {
	        Member("document", &FSceneComponentStructureRequest::Document, {.bRequired = true}),
	        Member("revision", &FSceneComponentStructureRequest::Revision, {.bRequired = true}),
	        Member("handles", &FSceneComponentStructureRequest::Handles, {.bRequired = true}),
	        Member("component", &FSceneComponentStructureRequest::Component, {.bRequired = true}),
	        Member("type", &FSceneComponentStructureRequest::Type, {.bRequired = true}),
	        Member("remove", &FSceneComponentStructureRequest::bRemove, {.bRequired = true}),
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneComponentInfo>()
{
	static const auto Type = MakeRecord<FSceneComponentInfo>(
	    "hyperion.scene.component.info", {
	                                         Member("component", &FSceneComponentInfo::Component, {.bRequired = true}),
	                                         Member("type", &FSceneComponentInfo::Type, {.bRequired = true}),
	                                         Member("label", &FSceneComponentInfo::Label, {.bRequired = true}),
	                                         Member("required", &FSceneComponentInfo::bRequired, {.bRequired = true}),
	                                     });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneComponentList>()
{
	static const auto Type =
	    MakeRecord<FSceneComponentList>("hyperion.scene.component.list",
	                                    {
	                                        Member("components", &FSceneComponentList::Components, {.bRequired = true}),
	                                    });
	return Type;
}

} // namespace Hyperion
