#pragma once
#include "Hyperion/Reflection/ArchiveNode.h"
#include <string_view>

namespace Hyperion
{
struct FJsonLimits
{
	std::size_t MaxBytes = 1024 * 1024;
	std::size_t MaxNodes = 65536;
	unsigned MaxDepth = 48;
};

// Plain JSON values only. Bulk data must first be projected through a reflected wire shape.
FArchiveNode ParseJson(std::string_view InText, FJsonLimits InLimits = {});
std::string WriteJson(const FArchiveNode& InValue, FJsonLimits InLimits = {});
} // namespace Hyperion
