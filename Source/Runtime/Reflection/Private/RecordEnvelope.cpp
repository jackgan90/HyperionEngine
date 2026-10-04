#include "Hyperion/Reflection/RecordEnvelope.h"
#include "Hyperion/Reflection/Record.h"

namespace Hyperion
{
namespace
{
constexpr char TypeKey[] = "type";
constexpr char VersionKey[] = "version";
} // namespace

FArchiveNode MakeRecordEnvelope(std::string InTypeId, std::uint32_t InVersion, FArchiveNode::FObject InFields)
{
	return FArchiveNode(FArchiveNode::FObject{{TypeKey, FArchiveNode(std::move(InTypeId))},
	                                          {VersionKey, WriteValue(InVersion)},
	                                          {RecordFieldsKey, FArchiveNode(std::move(InFields))}});
}

const FArchiveNode::FObject& RecordFields(const FArchiveNode& InNode)
{
	return std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InNode.Value).at(RecordFieldsKey).Value);
}

FArchiveNode::FObject& RecordFields(FArchiveNode& InNode)
{
	return std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InNode.Value).at(RecordFieldsKey).Value);
}

const std::string& RecordTypeId(const FArchiveNode& InNode)
{
	return std::get<std::string>(std::get<FArchiveNode::FObject>(InNode.Value).at(TypeKey).Value);
}

std::uint32_t RecordVersion(const FArchiveNode& InNode)
{
	return ReadValue<std::uint32_t>(std::get<FArchiveNode::FObject>(InNode.Value).at(VersionKey));
}

bool HasRecordEnvelopeShape(const FArchiveNode& InNode)
{
	const auto* Object = std::get_if<FArchiveNode::FObject>(&InNode.Value);
	if (!Object)
	{
		return false;
	}
	const auto Type = Object->find(TypeKey);
	const auto Fields = Object->find(RecordFieldsKey);
	return Type != Object->end() && Fields != Object->end() && Object->contains(VersionKey) &&
	       std::holds_alternative<std::string>(Type->second.Value) &&
	       std::holds_alternative<FArchiveNode::FObject>(Fields->second.Value);
}
} // namespace Hyperion
