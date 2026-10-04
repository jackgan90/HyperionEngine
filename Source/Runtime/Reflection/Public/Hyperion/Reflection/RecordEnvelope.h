#pragma once
#include "Hyperion/Reflection/ArchiveNode.h"

namespace Hyperion
{
// Existing inspection paths navigate archive nodes through this envelope key.
inline constexpr char RecordFieldsKey[] = "fields";

// Wraps fields without descriptor or domain validation, including unfinished inspection drafts.
FArchiveNode MakeRecordEnvelope(std::string InTypeId, std::uint32_t InVersion, FArchiveNode::FObject InFields);

// Access only the requested envelope part; missing keys or incorrect node kinds throw.
// References borrow from InNode and follow its lifetime and container/variant invalidation rules.
const FArchiveNode::FObject& RecordFields(const FArchiveNode& InNode);
FArchiveNode::FObject& RecordFields(FArchiveNode& InNode);
const std::string& RecordTypeId(const FArchiveNode& InNode);
std::uint32_t RecordVersion(const FArchiveNode& InNode);

// Shape only: string type, object fields and a present version of any kind.
// Does not establish a valid version, registered descriptor or valid domain value.
bool HasRecordEnvelopeShape(const FArchiveNode& InNode);
} // namespace Hyperion
