#include "Hyperion/AssetImport/ImportWorkspace.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FImportRequest>()
{
	static const auto Type = MakeRecord<FImportRequest>(
	    "automation.asset.import",
	    {Member("generation", &FImportRequest::Generation,
	            {.bRequired = true, .Description = "Current content.root.get generation."}),
	     Member("source", &FImportRequest::Source,
	            {.bRequired = true, .Description = "Source file accessible to the target process."}),
	     Member("output", &FImportRequest::Output,
	            {.bRequired = true, .Description = "Separate .hasset destination; mount permissions apply."}),
	     Member("library", &FImportRequest::Library,
	            {.Description = "Shared dependency directory; omitted defaults to output parent."}),
	     Member("name", &FImportRequest::Name,
	            {.Description = "Model or standalone image display name; with scene, model node name. Other sources "
	                            "retain their names."}),
	     Member("type", &FImportRequest::Type,
	            {.Description = "Optional source type ID; omitted infers from extension or JSON/native header."}),
	     Member("sourceRoot", &FImportRequest::SourceRoot,
	            {.Description = "Optional source namespace root; supply with sourceId."}),
	     Member("sourceId", &FImportRequest::SourceId,
	            {.Description = "Portable logical source namespace; supply with sourceRoot."}),
	     Member("scene", &FImportRequest::bScene,
	            {.Description = "Wrap a model as a scene using the existing publication policy."}),
	     Member("force", &FImportRequest::bForce,
	            {.Description = "Bypass freshness only; does not bypass identity or write checks."}),
	     Member("rootId", &FImportRequest::RootId,
	            {.Description =
	                 "Optional 32 lowercase hexadecimal reconstruction identity; must match an existing target."}),
	     Member("textureEncoding", &FImportRequest::TextureEncoding,
	            {.Description = "Standalone PNG/JPEG only: 0 linear, 1 sRGB. Omitted defaults to sRGB."}),
	     Member("sky", &FImportRequest::Sky,
	            {.Description = "Optional complete bake settings overriding HDR defaults or a sky recipe. Not "
	                            "applicable to serialized sky records."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportResult>()
{
	static const auto Type = MakeRecord<FImportResult>(
	    "automation.asset.import.result",
	    {Member("asset", &FImportResult::Asset), Member("writtenAssets", &FImportResult::WrittenAssets),
	     Member("upToDate", &FImportResult::bUpToDate), Member("task", &FImportResult::Task),
	     Member("warning", &FImportResult::Warning,
	            {.Description =
	                 "Publication committed; optional index refresh diagnostic. Do not blindly repeat the import."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportValidation>()
{
	static const auto Type = MakeRecord<FImportValidation>(
	    "asset.import.validation",
	    {Member("source", &FImportValidation::Source), Member("output", &FImportValidation::Output),
	     Member("library", &FImportValidation::Library), Member("type", &FImportValidation::Type)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportCapability>()
{
	static const auto Type = MakeRecord<FImportCapability>("asset.import.capability",
	                                                       {Member("type", &FImportCapability::Type),
	                                                        Member("extensions", &FImportCapability::Extensions),
	                                                        Member("description", &FImportCapability::Description)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportCapabilities>()
{
	static const auto Type = MakeRecord<FImportCapabilities>(
	    "asset.import.capabilities",
	    {Member("formats", &FImportCapabilities::Formats), Member("cancellable", &FImportCapabilities::bCancellable)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportTaskQuery>()
{
	static const auto Type = MakeRecord<FImportTaskQuery>(
	    "asset.import.task-query",
	    {Member("task", &FImportTaskQuery::Task,
	            {.bRequired = true,
	             .Description = "Application import task ID from asset.import.tasks; not a session job ID."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportTaskListRequest>()
{
	static const auto Type = MakeRecord<FImportTaskListRequest>(
	    "asset.import.task-list-request",
	    {Member("offset", &FImportTaskListRequest::Offset),
	     Member("limit", &FImportTaskListRequest::Limit, {.Description = "1-32 records, newest first. Default 16."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportTaskInfo>()
{
	static const auto Type = MakeRecord<FImportTaskInfo>(
	    "asset.import.task-info",
	    {Member("task", &FImportTaskInfo::Task), Member("generation", &FImportTaskInfo::Generation),
	     Member("source", &FImportTaskInfo::Source), Member("output", &FImportTaskInfo::Output),
	     Member("status", &FImportTaskInfo::Status,
	            {.Description = "running, completed or failed. Accepted publication is not cancellable."}),
	     Member("result", &FImportTaskInfo::Result), Member("error", &FImportTaskInfo::Error)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportTaskList>()
{
	static const auto Type = MakeRecord<FImportTaskList>(
	    "asset.import.task-list", {Member("tasks", &FImportTaskList::Tasks), Member("total", &FImportTaskList::Total)});
	return Type;
}
} // namespace Hyperion
