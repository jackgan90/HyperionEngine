#include "AssetOperations.h"

namespace Hyperion
{
namespace
{
struct FPrimitiveQuery
{
	std::string Document;
	std::uint64_t Generation{};
	std::uint32_t Offset{};
	std::uint32_t Limit = 50;
};

struct FPrimitivePage
{
	std::vector<FModelPrimitiveInfo> Values;
	std::uint64_t Total{};
	std::optional<std::uint32_t> Next;
};

struct FPrimitiveEdit
{
	std::string Document;
	std::uint64_t Generation{};
	std::uint32_t Offset{};
	std::vector<FModelPrimitiveInfo> Values;
};
} // namespace

template<> const FRecordDescriptor& RecordType<FPrimitiveQuery>()
{
	static const auto Type = MakeRecord<FPrimitiveQuery>(
	    "automation.model.primitives.query",
	    {Member("document", &FPrimitiveQuery::Document, {.bRequired = true}),
	     Member("generation", &FPrimitiveQuery::Generation, {.bRequired = true}),
	     Member("offset", &FPrimitiveQuery::Offset), Member("limit", &FPrimitiveQuery::Limit)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FPrimitivePage>()
{
	static const auto Type =
	    MakeRecord<FPrimitivePage>("automation.model.primitives.page",
	                               {Member("values", &FPrimitivePage::Values), Member("total", &FPrimitivePage::Total),
	                                Member("next", &FPrimitivePage::Next)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FPrimitiveEdit>()
{
	static const auto Type = MakeRecord<FPrimitiveEdit>(
	    "automation.model.primitives.edit",
	    {Member("document", &FPrimitiveEdit::Document, {.bRequired = true}),
	     Member("generation", &FPrimitiveEdit::Generation, {.bRequired = true}),
	     Member("offset", &FPrimitiveEdit::Offset), Member("values", &FPrimitiveEdit::Values, {.bRequired = true})});
	return Type;
}

void RegisterModelProperties(FOperationCatalog& InCatalog, FAssetAutomation* InProvider)
{
	FOperationInfo Info;
	Info.Id = "model.primitives.get";
	Info.Owner = "automation-assets";
	Info.Summary = "Read model primitive names and material slots";
	Info.Description = "Pages of 1-100 entries. Geometry and identity are read-only; no bulk data is returned.";
	Info.bReadOnly = true;
	Info.Effects = "Reads current draft.";
	Info.Completion = "Current Main snapshot.";
	Info.Unavailable = InProvider ? "" : "Asset workspace provider unavailable.";
	const FPrimitiveQuery QueryExample{"document-from-open", 1};
	Info.Example = WriteRecordWire(RecordType<FPrimitiveQuery>(), &QueryExample);
	InCatalog.Register(MakeOperation<FPrimitiveQuery, FPrimitivePage>(
	    Info,
	    [InProvider](const auto& InRequest)
	    {
		    if (!InRequest.Limit || InRequest.Limit > 100)
		    {
			    throw std::invalid_argument("Page limit must be 1-100");
		    }
		    const auto Values = InProvider->ModelPrimitives({InRequest.Document, InRequest.Generation});
		    const auto Begin = std::min(std::size_t(InRequest.Offset), Values.size());
		    const auto End = std::min(Begin + InRequest.Limit, Values.size());
		    return FPrimitivePage{{Values.begin() + Begin, Values.begin() + End},
		                          Values.size(),
		                          End < Values.size() ? std::optional(static_cast<std::uint32_t>(End)) : std::nullopt};
	    }));
	Info.Id = "model.primitives.set";
	Info.Summary = "Edit model primitive names and material slots";
	Info.Description = "Replace 1-100 entries at offset. Read first and retain id/vertices/indices. Material is an "
	                   "existing materialSlots index or -1.";
	Info.bReadOnly = false;
	Info.Effects = "One shared draft/history transaction. Save explicitly.";
	const FPrimitiveEdit EditExample{"document-from-open", 1, 0, {}};
	Info.Example = WriteRecordWire(RecordType<FPrimitiveEdit>(), &EditExample);
	InCatalog.Register(MakeOperation<FPrimitiveEdit, FAssetDocumentInfo>(
	    Info,
	    [InProvider](const auto& InRequest)
	    {
		    return InProvider->SetPrimitives({InRequest.Document, InRequest.Generation}, InRequest.Offset,
		                                     InRequest.Values);
	    }));
}
} // namespace Hyperion
