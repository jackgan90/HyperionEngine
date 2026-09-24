#include "AssetOperations.h"
#include "Hyperion/AssetEditing/MaterialNumeric.h"

namespace Hyperion
{
namespace
{
struct FNumericQuery
{
	std::string Document;
	std::uint64_t Generation{};
	std::string Name;
};

struct FNumericEdit
{
	std::string Document;
	std::uint64_t Generation{};
	std::vector<FMaterialNumericEdit> Edits;
};
} // namespace

template<> const FRecordDescriptor& RecordType<FNumericQuery>()
{
	static const auto Type = MakeRecord<FNumericQuery>(
	    "automation.material.numeric.query",
	    {Member("document", &FNumericQuery::Document, {.bRequired = true}),
	     Member("generation", &FNumericQuery::Generation, {.bRequired = true}),
	     Member("name", &FNumericQuery::Name,
	            {.bRequired = true,
	             .Description = "Exact numeric parameter name; discover through material.parameters.get."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FNumericEdit>()
{
	static const auto Type = MakeRecord<FNumericEdit>(
	    "automation.material.numeric.edit",
	    {Member("document", &FNumericEdit::Document, {.bRequired = true}),
	     Member("generation", &FNumericEdit::Generation, {.bRequired = true}),
	     Member("edits", &FNumericEdit::Edits,
	            {.bRequired = true,
	             .Description = "1-100 distinct numeric parameters; all validated before one transaction."})});
	return Type;
}

FMaterialNumericInfo FAssetAutomation::MaterialNumeric(const FAssetMutationRequest& InRequest,
                                                       const std::string& InName)
{
	const auto Entry = Find(InRequest.Document);
	if (Entry->Document->Loaded().Type->CppType != typeid(FMaterialAsset))
	{
		throw FAutomationError("unsupported_type", "Numeric parameter operations require a material document");
	}
	if (Entry->Document->Generation() != InRequest.Generation)
	{
		throw FAutomationError("stale_revision", "Asset changed; query asset.info");
	}
	return DescribeMaterialNumeric(ReadValue<FMaterialAsset>(Entry->Document->Snapshot()), InName);
}

FAssetDocumentInfo FAssetAutomation::SetMaterialNumeric(const FAssetMutationRequest& InRequest,
                                                        const std::vector<FMaterialNumericEdit>& InEdits)
{
	const auto Entry = Edit(InRequest.Document, InRequest.Generation);
	if (Entry->Document->Loaded().Type->CppType != typeid(FMaterialAsset))
	{
		throw FAutomationError("unsupported_type", "Numeric parameter operations require a material document");
	}
	const auto Values = PrepareMaterialNumeric(ReadValue<FMaterialAsset>(Entry->Document->Snapshot()), InEdits);
	CommitAssetField(*Entry->Document, "values", WriteValue(Values));
	return Describe(*Entry);
}

void RegisterMaterialNumeric(FOperationCatalog& InCatalog, FAssetAutomation* InProvider)
{
	FOperationInfo Info;
	Info.Id = "material.numeric.get";
	Info.Owner = "automation-assets";
	Info.Summary = "Read a material numeric parameter as ordinary numbers";
	Info.Description =
	    "Discover names through material.parameters.get. Returns effective override or default; scalar/vector/matrix "
	    "numeric parameters only. Resource and aggregate parameters use material.values.";
	Info.Effects = "Reads the draft; no mutation.";
	Info.Completion = "Current Main snapshot.";
	Info.bReadOnly = true;
	Info.Keywords = {"material", "parameter", "float", "integer", "color", "edit"};
	Info.Unavailable = InProvider ? "" : "Asset provider unavailable.";
	const FNumericQuery Query{"document-from-open", 1, "RoughnessFactor"};
	Info.Example = WriteRecordWire(RecordType<FNumericQuery>(), &Query);
	InCatalog.Register(MakeOperation<FNumericQuery, FMaterialNumericInfo>(
	    Info,
	    [InProvider](const auto& InRequest)
	    {
		    return InProvider->MaterialNumeric({InRequest.Document, InRequest.Generation}, InRequest.Name);
	    }));
	Info.Id = "material.numeric.set";
	Info.Summary = "Edit material numbers without bit encoding";
	Info.Description = "Updates 1-100 existing numeric parameters atomically. Ordinary numbers, row-major "
	                   "rows*columns; boolean 0/1. Uses the same validation, normalized PBR clamping, history and "
	                   "persistence as GUI. Query numeric.get for stored float32 values; save explicitly.";
	Info.Effects = "One shared material values transaction; no disk write.";
	Info.bReadOnly = false;
	const FNumericEdit Edit{Query.Document, 1, {{"RoughnessFactor", {0.5}}}};
	Info.Example = WriteRecordWire(RecordType<FNumericEdit>(), &Edit);
	InCatalog.Register(MakeOperation<FNumericEdit, FAssetDocumentInfo>(
	    Info,
	    [InProvider](const auto& InRequest)
	    {
		    return InProvider->SetMaterialNumeric({InRequest.Document, InRequest.Generation}, InRequest.Edits);
	    }));
}
} // namespace Hyperion
