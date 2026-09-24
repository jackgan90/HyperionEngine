#include "AssetOperations.h"
#include "Hyperion/Environment/SkyAsset.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
namespace
{
struct FAssetPropertyQuery
{
	std::string Document;
	std::uint64_t Generation{};
	std::uint32_t Offset{};
	std::uint32_t Limit = 50;
};

const FRecordDescriptor& PropertyQueryType()
{
	static const auto Type = MakeRecord<FAssetPropertyQuery>(
	    "automation.asset.property.query",
	    {Member("document", &FAssetPropertyQuery::Document, {.bRequired = true}),
	     Member("generation", &FAssetPropertyQuery::Generation,
	            {.bRequired = true, .Description = "Expected asset generation; restart pagination after an edit."}),
	     Member("offset", &FAssetPropertyQuery::Offset,
	            {.Description = "Sequence page offset; ignored for scalar/record values."}),
	     Member("limit", &FAssetPropertyQuery::Limit, {.Description = "Sequence page size, 1-100."})});
	return Type;
}

template<class T> struct TAssetMember;

template<class S, class V> struct TAssetMember<V S::*>
{
	using FSource = S;
	using FValue = V;
};

template<auto Member> struct TAssetFieldValue
{
	typename TAssetMember<decltype(Member)>::FValue Value;
	std::uint64_t Total{};
	std::optional<std::uint32_t> Next;
};

template<auto Member> struct TAssetFieldEdit
{
	std::string Document;
	std::uint64_t Generation{};
	typename TAssetMember<decltype(Member)>::FValue Value;
	std::optional<std::uint32_t> Offset;
};

template<auto Member> const FRecordDescriptor& FieldResult(const std::string& InId)
{
	using FResult = TAssetFieldValue<Member>;
	static const auto Type = MakeRecord<FResult>(InId + ".value", {Hyperion::Member("value", &FResult::Value),
	                                                               Hyperion::Member("total", &FResult::Total),
	                                                               Hyperion::Member("next", &FResult::Next)});
	return Type;
}

template<auto Member> const FRecordDescriptor& FieldRequest(const std::string& InId)
{
	using FRequest = TAssetFieldEdit<Member>;
	static const auto Type = MakeRecord<FRequest>(
	    InId + ".request",
	    {Hyperion::Member("document", &FRequest::Document, {.bRequired = true}),
	     Hyperion::Member("generation", &FRequest::Generation, {.bRequired = true}),
	     Hyperion::Member(
	         "value", &FRequest::Value,
	         {.bRequired = true,
	          .Description =
	              "Replacement field or sequence range. Read first; preserve read-only identity and topology."}),
	     Hyperion::Member("offset", &FRequest::Offset,
	                      {.Description = "For sequences, replace an existing range at this offset without resizing. "
	                                      "Omit for complete replacement."})});
	return Type;
}

FOperationInfo PropertyInfo(const std::string& InId, bool bInReadOnly, FAssetAutomation* InProvider)
{
	FOperationInfo Info;
	Info.Id = InId;
	Info.Summary = bInReadOnly ? "Read " + InId : "Edit " + InId;
	Info.Description = "Uses the same asset field validation/history as the GUI. Model identity/topology and material "
	                   "parameter declarations are immutable. Reference changes load and validate dependency "
	                   "type/dimension before commit. Read the field first and retain unchanged entries.";
	Info.Owner = "automation-assets";
	Info.bReadOnly = bInReadOnly;
	Info.Effects = bInReadOnly ? "Reads draft metadata without bulk geometry or pixels."
	                           : "One shared asset history transaction; no disk write. Save explicitly.";
	Info.Completion = bInReadOnly
	                      ? "Current Main snapshot."
	                      : "References validated and draft committed on Main; preview GPU readiness is separate.";
	Info.Keywords = {"asset", "model", "material", "property", "reference"};
	Info.Unavailable = InProvider ? "" : "Asset workspace provider unavailable.";
	return Info;
}

template<auto Member>
void RegisterField(FOperationCatalog& InCatalog, FAssetAutomation* InProvider, std::string InId, std::string InField,
                   bool bInWritable = true)
{
	using FSource = typename TAssetMember<decltype(Member)>::FSource;
	using FValue = typename TAssetMember<decltype(Member)>::FValue;
	using FResult = TAssetFieldValue<Member>;
	using FRequest = TAssetFieldEdit<Member>;
	const auto TypeId = RecordType<FSource>().Id;
	const auto& ResultType = FieldResult<Member>(InId);
	const auto& RequestType = FieldRequest<Member>(InId);
	auto Get = PropertyInfo(InId + ".get", true, InProvider);
	const FAssetPropertyQuery Example{"document-from-open", 1};
	Get.Example = WriteRecordWire(PropertyQueryType(), &Example);
	InCatalog.Register({std::move(Get), &PropertyQueryType(), &ResultType, false,
	                    [InProvider, TypeId, InField, Result = &ResultType](const void* InRequest)
	                    {
		                    const auto& Request = *static_cast<const FAssetPropertyQuery*>(InRequest);
		                    if (InProvider->Info({Request.Document}).Generation != Request.Generation)
		                    {
			                    throw FAutomationError("stale_revision", "Asset changed; restart the property query");
		                    }
		                    if (!Request.Limit || Request.Limit > 100)
		                    {
			                    throw std::invalid_argument("Property page limit must be 1-100");
		                    }
		                    FResult Value{ReadValue<FValue>(InProvider->ReadField(Request.Document, TypeId, InField))};
		                    if constexpr (requires { typename FValue::allocator_type; })
		                    {
			                    Value.Total = Value.Value.size();
			                    const auto Begin = std::min(std::size_t(Request.Offset), Value.Value.size());
			                    const auto End = std::min(Begin + Request.Limit, Value.Value.size());
			                    FValue Page(Value.Value.begin() + Begin, Value.Value.begin() + End);
			                    if (End < Value.Value.size())
			                    {
				                    Value.Next = static_cast<std::uint32_t>(End);
			                    }
			                    Value.Value = std::move(Page);
		                    }
		                    return FOperationTask{WriteRecordWire(*Result, &Value)};
	                    }});
	if (!bInWritable)
	{
		return;
	}
	auto Set = PropertyInfo(InId + ".set", false, InProvider);
	const FRequest SetExample{Example.Document, 1, {}};
	Set.Example = WriteRecordWire(RequestType, &SetExample);
	InCatalog.Register(
	    {std::move(Set), &RequestType, &RecordType<FAssetDocumentInfo>(), true,
	     [InProvider, TypeId, InField](const void* InRequest)
	     {
		     const auto& Request = *static_cast<const FRequest*>(InRequest);
		     auto Value = Request.Value;
		     if (Request.Offset)
		     {
			     if constexpr (requires { typename FValue::allocator_type; })
			     {
				     auto Full = ReadValue<FValue>(InProvider->ReadField(Request.Document, TypeId, InField));
				     if (*Request.Offset > Full.size() || Value.size() > Full.size() - *Request.Offset)
				     {
					     throw std::invalid_argument("Replacement range is outside the field");
				     }
				     std::copy(Value.begin(), Value.end(), Full.begin() + *Request.Offset);
				     Value = std::move(Full);
			     }
			     else
			     {
				     throw std::invalid_argument("Offset requires a sequence field");
			     }
		     }
		     auto Pending =
		         InProvider->SetField(Request.Document, Request.Generation, TypeId, InField, WriteValue(Value));
		     return FOperationTask{{},
		                           [Poll = std::move(Pending.Poll)]() -> std::optional<FArchiveNode>
		                           {
			                           if (const auto Result = Poll())
			                           {
				                           return WriteRecordWire(RecordType<FAssetDocumentInfo>(), &*Result);
			                           }
			                           return {};
		                           },
		                           std::move(Pending.Cancel)};
	     }});
}
} // namespace

void RegisterAssetProperties(FOperationCatalog& InCatalog, FAssetAutomation* InProvider)
{
	RegisterModelProperties(InCatalog, InProvider);
	RegisterMaterialNumeric(InCatalog, InProvider);
	RegisterTextureSamples(InCatalog, InProvider);
	RegisterField<&FModelAsset::Nodes>(InCatalog, InProvider, "model.nodes", "nodes");
	RegisterField<&FModelAsset::MaterialSlots>(InCatalog, InProvider, "model.material_slots", "materialSlots");
	RegisterField<&FMaterialAsset::Values>(InCatalog, InProvider, "material.values", "values");
	RegisterField<&FMaterialAsset::Parameters>(InCatalog, InProvider, "material.parameters", "parameters", false);
	RegisterField<&FMaterialAsset::Passes>(InCatalog, InProvider, "material.passes", "passes", false);
	RegisterField<&FModelAsset::Roots>(InCatalog, InProvider, "model.roots", "roots", false);
	RegisterField<&FSkyAsset::Radiance>(InCatalog, InProvider, "sky.radiance", "radiance", false);
	RegisterField<&FSkyAsset::Specular>(InCatalog, InProvider, "sky.specular", "specular", false);
	RegisterField<&FSkyAsset::Brdf>(InCatalog, InProvider, "sky.brdf", "brdf", false);
	RegisterField<&FSkyAsset::Irradiance>(InCatalog, InProvider, "sky.irradiance", "irradiance", false);
	RegisterField<&FSkyAsset::Convention>(InCatalog, InProvider, "sky.convention", "convention", false);
	RegisterField<&FTextureAsset::Dimension>(InCatalog, InProvider, "texture.dimension", "dimension", false);
	RegisterField<&FTextureAsset::Format>(InCatalog, InProvider, "texture.format", "format", false);
	RegisterField<&FTextureAsset::Encoding>(InCatalog, InProvider, "texture.encoding", "encoding", false);
}
} // namespace Hyperion
