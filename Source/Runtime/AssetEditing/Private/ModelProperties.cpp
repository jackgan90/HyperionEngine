#include "Hyperion/AssetEditing/ModelProperties.h"
#include "Hyperion/AssetEditing/AssetFieldPolicy.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
std::vector<FModelPrimitiveInfo> DescribeModelPrimitives(const FAssetEditDocument& InDocument)
{
	if (InDocument.Loaded().Type->CppType != typeid(FModelAsset))
	{
		throw std::invalid_argument("Model document required");
	}
	std::vector<FModelPrimitiveInfo> Result;
	const auto Policy = ResolveAssetFieldPolicy(*InDocument.Loaded().Type, &FModelAsset::Primitives);
	const auto& Primitives = std::get<FArchiveNode::FArray>(InDocument.Get(Policy.Field().FieldId).Value);
	const auto Model = InDocument.Loaded().As<FModelAsset>();
	for (std::size_t Index = 0; Index < Primitives.size(); ++Index)
	{
		const auto& Fields = RecordFields(Primitives[Index]);
		Result.push_back({ReadValue<std::string>(Fields.at("id")), ReadValue<std::string>(Fields.at("name")),
		                  ReadValue<std::int32_t>(Fields.at("material")),
		                  Model->Primitives.at(Index).Positions.size() / 3,
		                  Model->Primitives.at(Index).Indices.size()});
	}
	return Result;
}

void SetModelPrimitives(FAssetEditDocument& InDocument, const std::vector<FModelPrimitiveInfo>& InValues,
                        std::uint64_t InInteraction)
{
	const auto Before = DescribeModelPrimitives(InDocument);
	if (Before.size() != InValues.size())
	{
		throw std::invalid_argument("Primitive count is fixed");
	}
	const auto SlotsPolicy = ResolveAssetFieldPolicy(*InDocument.Loaded().Type, &FModelAsset::MaterialSlots);
	const auto Slots = ReadValue<std::vector<FAssetRef>>(InDocument.Get(SlotsPolicy.Field().FieldId));
	const auto Policy = ResolveAssetFieldPolicy(*InDocument.Loaded().Type, &FModelAsset::Primitives);
	auto Primitives = InDocument.Get(Policy.Field().FieldId);
	auto& Array = std::get<FArchiveNode::FArray>(Primitives.Value);
	for (std::size_t Index = 0; Index < Before.size(); ++Index)
	{
		const auto& Value = InValues[Index];
		if (Value.Id != Before[Index].Id || Value.Vertices != Before[Index].Vertices ||
		    Value.Indices != Before[Index].Indices || Value.Material < -1 ||
		    (Value.Material >= 0 && std::size_t(Value.Material) >= Slots.size()))
		{
			throw std::invalid_argument(
			    "Primitive identity/geometry is immutable; material must select an existing slot or -1");
		}
		auto& Fields = RecordFields(Array[Index]);
		Fields.at("name") = WriteValue(Value.Name);
		Fields.at("material") = WriteValue(Value.Material);
	}
	InDocument.Apply(Policy.Field(), std::move(Primitives), InInteraction);
}

void CommitModelPrimitiveFields(FAssetEditDocument& InDocument, const FArchiveNode& InValue,
                                std::uint64_t InInteraction)
{
	auto Values = DescribeModelPrimitives(InDocument);
	const auto& Array = std::get<FArchiveNode::FArray>(InValue.Value);
	if (Array.size() != Values.size())
	{
		throw std::invalid_argument("Primitive count is fixed");
	}
	for (std::size_t Index = 0; Index < Array.size(); ++Index)
	{
		const auto& Fields = RecordFields(Array[Index]);
		Values[Index].Name = ReadValue<std::string>(Fields.at("name"));
		Values[Index].Material = ReadValue<std::int32_t>(Fields.at("material"));
	}
	SetModelPrimitives(InDocument, Values, InInteraction);
}

template<> const FRecordDescriptor& RecordType<FModelPrimitiveInfo>()
{
	static const auto Type = MakeRecord<FModelPrimitiveInfo>(
	    "hyperion.model.primitive.info",
	    {Member("id", &FModelPrimitiveInfo::Id,
	            {.bRequired = true, .Description = "Immutable stable primitive identity."}),
	     Member("name", &FModelPrimitiveInfo::Name), Member("material", &FModelPrimitiveInfo::Material),
	     Member("vertices", &FModelPrimitiveInfo::Vertices,
	            {.bRequired = true, .Description = "Read-only vertex count; retain from query."}),
	     Member("indices", &FModelPrimitiveInfo::Indices,
	            {.bRequired = true, .Description = "Read-only index count; retain from query."})});
	return Type;
}
} // namespace Hyperion
