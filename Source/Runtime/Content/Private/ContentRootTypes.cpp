#include "Hyperion/Content/ContentRootService.h"
#include "Hyperion/Reflection/RecordValue.h"

namespace Hyperion
{
namespace
{
const FRecordMemberOptions Generation{
    .bRequired = true, .Description = "Current root generation from content.root.get, as a decimal string."};
const FRecordMemberOptions Discard{
    .Description =
        "Explicitly discard unsaved documents. Pending edits and saves must finish first. Defaults to false."};
} // namespace

template<> const FRecordDescriptor& RecordType<FContentRootQuery>()
{
	static const auto Type = MakeRecord<FContentRootQuery>("hyperion.content.root.query", {});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FContentRootInfo>()
{
	static const auto Type = MakeRecord<FContentRootInfo>(
	    "hyperion.content.root.info",
	    {Member("directory", &FContentRootInfo::Directory,
	            {.Description = "Canonical local directory, or empty when Game is unmounted."}),
	     Member("generation", &FContentRootInfo::Generation), Member("readOnly", &FContentRootInfo::bReadOnly)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FContentRootRequest>()
{
	static const auto Type = MakeRecord<FContentRootRequest>(
	    "hyperion.content.root.set",
	    {Member("directory", &FContentRootRequest::Directory,
	            {.bRequired = true,
	             .Description = "Existing local asset directory. Relative paths use the server working directory; "
	                            "absolute paths are recommended."}),
	     Member("generation", &FContentRootRequest::Generation, Generation),
	     Member("readOnly", &FContentRootRequest::bReadOnly,
	            {.Description = "Mount Game read-only. Defaults to false."}),
	     Member("discard", &FContentRootRequest::bDiscard, Discard)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FContentRootClearRequest>()
{
	static const auto Type = MakeRecord<FContentRootClearRequest>(
	    "hyperion.content.root.clear", {Member("generation", &FContentRootClearRequest::Generation, Generation),
	                                    Member("discard", &FContentRootClearRequest::bDiscard, Discard)});
	return Type;
}
} // namespace Hyperion
