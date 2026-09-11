#pragma once
#include <cstddef>
#include <span>
#include <string>

namespace Hyperion
{
std::string ContentHash(std::span<const std::byte> InBytes);
std::string ContentHashParts(std::span<const std::span<const std::byte>> InParts);
std::string CreateIdentifier();
} // namespace Hyperion
