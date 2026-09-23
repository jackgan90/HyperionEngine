#include "Hyperion/AssetEditing/ModelProperties.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
namespace
{
FArchiveNode::FObject& PrimitiveFields(FArchiveNode& InNode)
{
	return std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InNode.Value).at("fields").Value);
}
} // namespace

std::vector<FModelPrimitiveInfo> DescribeModelPrimitives(const FAssetEditDocument& InDocument)
{
	if (InDocument.Loaded().Type->CppType != typeid(FModelAsset))
	{
		throw std::invalid_argument("Model document required");
	}
	std::vector<FModelPrimitiveInfo> Result;
	const auto& Primitives = std::get<FArchiveNode::FArray>(InDocument.Get("primitives").Value);
	const auto Model = InDocument.Loaded().As<FModelAsset>();
	for (std::size_t Index = 0; Index < Primitives.size(); ++Index)
	{
		const auto& Fields = std::get<FArchiveNode::FObject>(
		    std::get<FArchiveNode::FObject>(Primitives[Index].Value).at("fields").Value);
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
	const auto Slots = ReadValue<std::vector<FAssetRef>>(InDocument.Get("materialSlots"));
	auto Primitives = InDocument.Get("primitives");
	auto& Array = std::get<FArchiveNode::FArray>(Primitives.Value);
	bool bAffectsPreview{};
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
		auto& Fields = PrimitiveFields(Array[Index]);
		Fields.at("name") = WriteValue(Value.Name);
		Fields.at("material") = WriteValue(Value.Material);
		bAffectsPreview |= Value.Material != Before[Index].Material;
	}
	InDocument.Set("primitives", std::move(Primitives), InInteraction, bAffectsPreview);
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
		const auto& Fields =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Array[Index].Value).at("fields").Value);
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
