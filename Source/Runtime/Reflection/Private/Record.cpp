#include "Hyperion/Reflection/Record.h"

namespace Hyperion
{
FArchiveNode WriteRecord(const FRecordDescriptor& InType, const void* InObject)
{
	if (InType.Validate)
	{
		InType.Validate(InObject);
	}
	FArchiveNode::FObject Fields;
	for (const auto& Field : InType.Members)
	{
		if (!Fields.emplace(Field.Id, Field.Write(InObject)).second)
		{
			throw std::logic_error("Duplicate reflected member");
		}
	}
	return FArchiveNode(FArchiveNode::FObject{{"type", FArchiveNode(InType.Id)},
	                                          {"version", WriteValue(InType.Version)},
	                                          {"fields", FArchiveNode(std::move(Fields))}});
}

void ReadRecordFields(const FRecordDescriptor& InType, void* InObject, const FArchiveNode& InNode)
{
	const auto& Object = std::get<FArchiveNode::FObject>(InNode.Value);
	const auto Version = ReadValue<std::uint32_t>(Object.at("version"));
	if (ReadValue<std::string>(Object.at("type")) != InType.Id || !Version || Version > InType.Version)
	{
		throw std::runtime_error("Archive type or schema version mismatch: " + InType.Id);
	}
	const auto& Fields = std::get<FArchiveNode::FObject>(Object.at("fields").Value);
	for (const auto& Field : InType.Members)
	{
		auto It = Fields.find(Field.Id);
		if (It != Fields.end())
		{
			try
			{
				Field.Read(InObject, It->second);
			}
			catch (const std::exception& Error)
			{
				throw std::runtime_error(InType.Id + "." + Field.Id + ": " + Error.what());
			}
		}
	}
	if (InType.Validate)
	{
		InType.Validate(InObject);
	}
}

std::shared_ptr<void> ReadRecord(const FRecordDescriptor& InType, const FArchiveNode& InNode)
{
	auto Object = InType.Create();
	ReadRecordFields(InType, Object.get(), InNode);
	return Object;
}
} // namespace Hyperion
