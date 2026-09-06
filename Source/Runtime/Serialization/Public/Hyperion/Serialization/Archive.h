#pragma once
#include "Hyperion/Reflection/Record.h"
#include <span>

namespace Hyperion
{
std::vector<std::byte> EncodeArchive(const FArchiveNode& InNode);
FArchiveNode DecodeArchive(std::span<const std::byte> InBytes);

template<class T> std::vector<std::byte> Serialize(const T& InObject)
{
	return EncodeArchive(WriteValue(InObject));
}

template<class T> T Deserialize(std::span<const std::byte> InBytes)
{
	return ReadValue<T>(DecodeArchive(InBytes));
}
} // namespace Hyperion
