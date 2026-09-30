#include "Hyperion/AssetImport/ImportDraft.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FImportNodeEdit>()
{
	static const auto Type = MakeRecord<FImportNodeEdit>(
	    "asset.import.node-edit", {Member("id", &FImportNodeEdit::Id), Member("name", &FImportNodeEdit::Name),
	                               Member("local", &FImportNodeEdit::Local)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportPrimitiveEdit>()
{
	static const auto Type = MakeRecord<FImportPrimitiveEdit>("asset.import.primitive-edit",
	                                                          {Member("id", &FImportPrimitiveEdit::Id),
	                                                           Member("name", &FImportPrimitiveEdit::Name),
	                                                           Member("material", &FImportPrimitiveEdit::Material)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportPropertyEdits>()
{
	static const auto Type = MakeRecord<FImportPropertyEdits>(
	    "asset.import.property-edits",
	    {Member("name", &FImportPropertyEdits::Name), Member("nodes", &FImportPropertyEdits::Nodes),
	     Member("primitives", &FImportPropertyEdits::Primitives), Member("material", &FImportPropertyEdits::Material)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportDraftQuery>()
{
	static const auto Type = MakeRecord<FImportDraftQuery>(
	    "asset.import.draft-query",
	    {Member("draft", &FImportDraftQuery::Draft, {.bRequired = true}), Member("offset", &FImportDraftQuery::Offset),
	     Member("limit", &FImportDraftQuery::Limit, {.Description = "1-64 properties per page. Default 32."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportDraftMutation>()
{
	static const auto Type = MakeRecord<FImportDraftMutation>(
	    "asset.import.draft-mutation", {Member("draft", &FImportDraftMutation::Draft, {.bRequired = true}),
	                                    Member("generation", &FImportDraftMutation::Generation, {.bRequired = true})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportDraftEdit>()
{
	static const auto Type = MakeRecord<FImportDraftEdit>(
	    "asset.import.draft-edit", {Member("draft", &FImportDraftEdit::Draft, {.bRequired = true}),
	                                Member("generation", &FImportDraftEdit::Generation, {.bRequired = true}),
	                                Member("properties", &FImportDraftEdit::Properties)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportDraftHistory>()
{
	static const auto Type = MakeRecord<FImportDraftHistory>(
	    "asset.import.draft-history", {Member("draft", &FImportDraftHistory::Draft, {.bRequired = true}),
	                                   Member("generation", &FImportDraftHistory::Generation, {.bRequired = true}),
	                                   Member("action", &FImportDraftHistory::Action)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportDraftDiscard>()
{
	static const auto Type = MakeRecord<FImportDraftDiscard>(
	    "asset.import.draft-discard", {Member("draft", &FImportDraftDiscard::Draft, {.bRequired = true}),
	                                   Member("generation", &FImportDraftDiscard::Generation, {.bRequired = true}),
	                                   Member("discard", &FImportDraftDiscard::bDiscard)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportProductInfo>()
{
	static const auto Type = MakeRecord<FImportProductInfo>(
	    "asset.import.product-info",
	    {Member("name", &FImportProductInfo::Name), Member("type", &FImportProductInfo::Type),
	     Member("width", &FImportProductInfo::Width), Member("height", &FImportProductInfo::Height),
	     Member("mips", &FImportProductInfo::Mips), Member("format", &FImportProductInfo::Format)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportDraftInfo>()
{
	static const auto Type = MakeRecord<FImportDraftInfo>(
	    "asset.import.draft-info",
	    {Member("draft", &FImportDraftInfo::Draft),
	     Member("generation", &FImportDraftInfo::Generation),
	     Member("status", &FImportDraftInfo::Status),
	     Member("error", &FImportDraftInfo::Error),
	     Member("type", &FImportDraftInfo::Type),
	     Member("name", &FImportDraftInfo::Name),
	     Member("nameEditable", &FImportDraftInfo::bNameEditable),
	     Member("dirty", &FImportDraftInfo::bDirty),
	     Member("canUndo", &FImportDraftInfo::bCanUndo),
	     Member("canRedo", &FImportDraftInfo::bCanRedo),
	     Member("properties", &FImportDraftInfo::Properties),
	     Member("nodes", &FImportDraftInfo::Nodes),
	     Member("primitives", &FImportDraftInfo::Primitives),
	     Member("materialSlots", &FImportDraftInfo::MaterialSlots),
	     Member("material", &FImportDraftInfo::Material),
	     Member("products", &FImportDraftInfo::Products),
	     Member("dependencies", &FImportDraftInfo::Dependencies),
	     Member("diagnostics", &FImportDraftInfo::Diagnostics),
	     Member("totalNodes", &FImportDraftInfo::TotalNodes),
	     Member("totalPrimitives", &FImportDraftInfo::TotalPrimitives),
	     Member("totalMaterial", &FImportDraftInfo::TotalMaterial),
	     Member("totalProducts", &FImportDraftInfo::TotalProducts),
	     Member("totalDependencies", &FImportDraftInfo::TotalDependencies),
	     Member("totalMaterialSlots", &FImportDraftInfo::TotalMaterialSlots),
	     Member("totalDiagnostics", &FImportDraftInfo::TotalDiagnostics),
	     Member("vertices", &FImportDraftInfo::Vertices),
	     Member("triangles", &FImportDraftInfo::Triangles),
	     Member("boundsMin", &FImportDraftInfo::BoundsMin),
	     Member("boundsMax", &FImportDraftInfo::BoundsMax),
	     Member("width", &FImportDraftInfo::Width),
	     Member("height", &FImportDraftInfo::Height),
	     Member("mips", &FImportDraftInfo::Mips),
	     Member("format", &FImportDraftInfo::Format),
	     Member("encoding", &FImportDraftInfo::Encoding),
	     Member("details", &FImportDraftInfo::Details),
	     Member("sourceWidth", &FImportDraftInfo::SourceWidth),
	     Member("sourceHeight", &FImportDraftInfo::SourceHeight),
	     Member("sky", &FImportDraftInfo::Sky),
	     Member("dimension", &FImportDraftInfo::Dimension,
	            {.Description = "Texture dimensionality; applies to texture drafts."}),
	     Member("pixelBytes", &FImportDraftInfo::PixelBytes,
	            {.Description = "Total pixel payload bytes across all texture mips and faces; "
	                            "zero for non-texture drafts."})});
	return Type;
}

} // namespace Hyperion
