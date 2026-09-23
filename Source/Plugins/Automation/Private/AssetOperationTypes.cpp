#include "AssetOperations.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FAssetDocumentList>()
{
	static const auto Type = MakeRecord<FAssetDocumentList>("automation.asset.documents",
	                                                        {Member("documents", &FAssetDocumentList::Documents),
	                                                         Member("total", &FAssetDocumentList::Total),
	                                                         Member("nextOffset", &FAssetDocumentList::Next)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetWorkspaceQuery>()
{
	static const auto Type = MakeRecord<FAssetWorkspaceQuery>(
	    "automation.asset.workspace.query",
	    {Member("offset", &FAssetWorkspaceQuery::Offset), Member("limit", &FAssetWorkspaceQuery::Limit)});
	return Type;
}

namespace
{
FRecordMemberOptions Required(std::string InDescription)
{
	return {.bRequired = true, .Description = std::move(InDescription)};
}

const auto Document = Required("Opaque document ID returned by asset.open in this session.");
const auto Generation =
    Required("Current document generation as a decimal string. Re-query after any edit, undo or redo.");
} // namespace

template<> const FRecordDescriptor& RecordType<FAssetOpenRequest>()
{
	static const auto Type = MakeRecord<FAssetOpenRequest>(
	    "automation.asset.open",
	    {Member("path", &FAssetOpenRequest::Path,
	            Required("Mounted path to a native .hasset model, material, texture or sky."))});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetDocumentRequest>()
{
	static const auto Type = MakeRecord<FAssetDocumentRequest>(
	    "automation.asset.document", {Member("document", &FAssetDocumentRequest::Document, Document)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetMutationRequest>()
{
	static const auto Type = MakeRecord<FAssetMutationRequest>(
	    "automation.asset.mutation", {Member("document", &FAssetMutationRequest::Document, Document),
	                                  Member("generation", &FAssetMutationRequest::Generation, Generation)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetRenameRequest>()
{
	static const auto Type = MakeRecord<FAssetRenameRequest>(
	    "automation.asset.rename",
	    {Member("document", &FAssetRenameRequest::Document, Document),
	     Member("generation", &FAssetRenameRequest::Generation, Generation),
	     Member("name", &FAssetRenameRequest::Name,
	            Required("New display name; the asset identity and file path stay stable."))});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetEncodingRequest>()
{
	static const auto Type = MakeRecord<FAssetEncodingRequest>(
	    "automation.texture.encoding",
	    {Member("document", &FAssetEncodingRequest::Document, Document),
	     Member("generation", &FAssetEncodingRequest::Generation, Generation),
	     Member("encoding", &FAssetEncodingRequest::Encoding,
	            Required(
	                "0 = Linear; 1 = sRGB. Rebuilds mip levels from unchanged base pixels. Only 2D RGBA8 textures."))});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetCloseRequest>()
{
	static const auto Type = MakeRecord<FAssetCloseRequest>(
	    "automation.asset.close", {Member("document", &FAssetCloseRequest::Document, Document),
	                               Member("generation", &FAssetCloseRequest::Generation, Generation),
	                               Member("discard", &FAssetCloseRequest::bDiscard,
	                                      {.Description = "Explicitly discard unsaved edits. Defaults to false."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetDocumentInfo>()
{
	static const auto Type = MakeRecord<FAssetDocumentInfo>(
	    "automation.asset.info",
	    {Member("document", &FAssetDocumentInfo::Document), Member("path", &FAssetDocumentInfo::Path),
	     Member("assetId", &FAssetDocumentInfo::AssetId), Member("type", &FAssetDocumentInfo::Type),
	     Member("name", &FAssetDocumentInfo::Name), Member("generation", &FAssetDocumentInfo::Generation),
	     Member("dirty", &FAssetDocumentInfo::bDirty), Member("canUndo", &FAssetDocumentInfo::bCanUndo),
	     Member("canRedo", &FAssetDocumentInfo::bCanRedo), Member("readOnly", &FAssetDocumentInfo::bReadOnly),
	     Member("saving", &FAssetDocumentInfo::bSaving), Member("editing", &FAssetDocumentInfo::bEditing),
	     Member("diskRevision", &FAssetDocumentInfo::DiskRevision), Member("fields", &FAssetDocumentInfo::Fields),
	     Member("state", &FAssetDocumentInfo::State,
	            {.Description =
	                 "ready, loading or failed. Non-ready entries have generation 0; close then reopen to retry."}),
	     Member("error", &FAssetDocumentInfo::Error), Member("active", &FAssetDocumentInfo::bActive)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetCloseResult>()
{
	static const auto Type =
	    MakeRecord<FAssetCloseResult>("automation.asset.closed", {Member("closed", &FAssetCloseResult::bClosed)});
	return Type;
}
} // namespace Hyperion
