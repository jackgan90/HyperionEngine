#pragma once
#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Record.h"

namespace Hyperion
{
class FWireError : public std::invalid_argument
{
public:
	FWireError(std::string InPath, std::string InMessage);
	std::string Path;
};

// API projection, independent from persistence envelopes. Non-persistent fields are omitted.
// Strict reads reject aliases/unknown keys. 64-bit integers use canonical decimal strings.
FArchiveNode RecordWireSchema(const FRecordDescriptor& InType);
FArchiveNode WriteRecordWire(const FRecordDescriptor& InType, const void* InObject);
std::shared_ptr<void> ReadRecordWire(const FRecordDescriptor& InType, const FArchiveNode& InValue);
FArchiveNode WriteValueWire(const FRecordValueShape& InShape, const FArchiveNode& InArchive);
FArchiveNode ReadValueWire(const FRecordValueShape& InShape, const FArchiveNode& InValue, std::string_view InPath = {});
} // namespace Hyperion
