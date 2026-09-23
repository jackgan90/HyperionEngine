#include "Hyperion/SceneEditing/SceneAuthoring.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FSceneSelectionRequest>()
{
	static const auto Type = MakeRecord<FSceneSelectionRequest>(
	    "hyperion.scene.selection.request",
	    {
	        Member("document", &FSceneSelectionRequest::Document,
	               {.bRequired = true, .Description = "Current document from scene.info."}),
	        Member("revision", &FSceneSelectionRequest::Revision,
	               {.bRequired = true, .Description = "Expected scene revision."}),
	        Member("handles", &FSceneSelectionRequest::Handles,
	               {.bRequired = true,
	                .Description = "Ordered distinct handles; last is primary. Empty clears selection. Maximum 128."}),
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneSelectionInfo>()
{
	static const auto Type = MakeRecord<FSceneSelectionInfo>(
	    "hyperion.scene.selection",
	    {
	        Member("document", &FSceneSelectionInfo::Document,
	               {.bRequired = true, .Description = "Current document from scene.info."}),
	        Member("revision", &FSceneSelectionInfo::Revision,
	               {.bRequired = true, .Description = "Expected scene revision."}),
	        Member("handles", &FSceneSelectionInfo::Handles, {.bRequired = true, .Description = ""}),
	        Member("primary", &FSceneSelectionInfo::Primary, {.bRequired = true, .Description = ""}),
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneMetadataEdit>()
{
	static const auto Type = MakeRecord<FSceneMetadataEdit>(
	    "hyperion.scene.metadata.edit",
	    {
	        Member("handle", &FSceneMetadataEdit::Handle, {.bRequired = true, .Description = ""}),
	        Member("name", &FSceneMetadataEdit::Name, {.Description = "Omit or null to retain the name."}),
	        Member("enabled", &FSceneMetadataEdit::Enabled, {.Description = "Omit or null to retain enabled state."}),
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneMetadataRequest>()
{
	static const auto Type = MakeRecord<FSceneMetadataRequest>(
	    "hyperion.scene.metadata.request",
	    {
	        Member("document", &FSceneMetadataRequest::Document,
	               {.bRequired = true, .Description = "Current document from scene.info."}),
	        Member("revision", &FSceneMetadataRequest::Revision,
	               {.bRequired = true, .Description = "Expected scene revision."}),
	        Member("edits", &FSceneMetadataRequest::Edits,
	               {.bRequired = true, .Description = "1-128 distinct nodes, validated atomically."}),
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneCreateRequest>()
{
	static const auto Type = MakeRecord<FSceneCreateRequest>(
	    "hyperion.scene.create.request",
	    {
	        Member("document", &FSceneCreateRequest::Document,
	               {.bRequired = true, .Description = "Current document from scene.info."}),
	        Member("revision", &FSceneCreateRequest::Revision,
	               {.bRequired = true, .Description = "Expected scene revision."}),
	        Member("name", &FSceneCreateRequest::Name, {.Description = "Display name."}),
	        Member("parent", &FSceneCreateRequest::Parent, {.Description = "Parent handle or null for a root."}),
	        Member("local", &FSceneCreateRequest::Local, {.Description = "Finite local affine matrix."}),
	        Member(
	            "components", &FSceneCreateRequest::Components,
	            {.Description = "Component type IDs. Transform is already present. Defaults must form a valid node."}),
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneReparentRequest>()
{
	static const auto Type = MakeRecord<FSceneReparentRequest>(
	    "hyperion.scene.reparent.request",
	    {
	        Member("document", &FSceneReparentRequest::Document,
	               {.bRequired = true, .Description = "Current document from scene.info."}),
	        Member("revision", &FSceneReparentRequest::Revision,
	               {.bRequired = true, .Description = "Expected scene revision."}),
	        Member("handle", &FSceneReparentRequest::Handle, {.bRequired = true, .Description = ""}),
	        Member("parent", &FSceneReparentRequest::Parent,
	               {.bRequired = true, .Description = "New parent or null for a root."}),
	        Member("keepWorld", &FSceneReparentRequest::bKeepWorld,
	               {.Description = "Preserve world transform; otherwise retain local transform."}),
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneSettings>()
{
	static const auto Type = MakeRecord<FSceneSettings>(
	    "hyperion.scene.settings", {
	                                   Member("defaultCamera", &FSceneSettings::DefaultCamera,
	                                          {.bRequired = true, .Description = "Runtime camera or null."}),
	                                   Member("mainDirectionalLight", &FSceneSettings::MainDirectionalLight,
	                                          {.bRequired = true, .Description = "Main light or null."}),
	                                   Member("environmentLight", &FSceneSettings::EnvironmentLight,
	                                          {.bRequired = true, .Description = "Environment light or null."}),
	                                   Member("initialView", &FSceneSettings::InitialView,
	                                          {.bRequired = true, .Description = "Saved browsing view or null."}),
	                               });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneSettingsRequest>()
{
	static const auto Type = MakeRecord<FSceneSettingsRequest>(
	    "hyperion.scene.settings.request",
	    {
	        Member("document", &FSceneSettingsRequest::Document,
	               {.bRequired = true, .Description = "Current document from scene.info."}),
	        Member("revision", &FSceneSettingsRequest::Revision,
	               {.bRequired = true, .Description = "Expected scene revision."}),
	        Member("settings", &FSceneSettingsRequest::Settings,
	               {.bRequired = true, .Description = "Complete replacement; query scene.settings.get first."}),
	    });
	return Type;
}

} // namespace Hyperion
