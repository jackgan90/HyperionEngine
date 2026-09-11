#pragma once
#include "Hyperion/Reflection/Record.h"
#include <span>

namespace Hyperion
{
struct FArchiveLimits
{
	std::size_t MaxBytes = 512u * 1024u * 1024u;
	std::size_t MaxAllocatedBytes = 1024u * 1024u * 1024u;
	std::size_t MaxNodes = 1000000;
	unsigned MaxDepth = 64;
};

// Hash the canonical encoding without copying bulk blocks.
std::string HashArchive(const FArchiveNode& InNode, FArchiveLimits InLimits = {});
std::vector<std::byte> EncodeArchive(const FArchiveNode& InNode, FArchiveLimits InLimits = {});
FArchiveNode DecodeArchive(std::span<const std::byte> InBytes, FArchiveLimits InLimits = {});
// Bulk views retain InBytes; Offset allows an enclosing native asset header.
FArchiveNode DecodeArchive(std::shared_ptr<const std::vector<std::byte>> InBytes, std::size_t InOffset = 0,
                           FArchiveLimits InLimits = {});

template<class T> std::vector<std::byte> Serialize(const T& InObject)
{
	return EncodeArchive(WriteValue(InObject));
}

template<class T> T Deserialize(std::span<const std::byte> InBytes)
{
	return ReadValue<T>(DecodeArchive(InBytes));
}
} // namespace Hyperion
