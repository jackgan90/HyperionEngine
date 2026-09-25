#pragma once
#include "Hyperion/Core/Profiling.h"
#include <string_view>

namespace Hyperion
{
struct FProfilingOptions
{
	std::uint32_t Mask{};
	std::uint64_t Start{};
	std::uint64_t Frames{};
	bool bWait{};
	bool bSampling{};
};

std::uint32_t ParseProfilingCategories(std::string_view InNames);
// Main startup before workload initialization; a missing collector is a bounded failure.
void InitializeProfilingSession(const FProfilingOptions& InOptions, bool bInDeferStart = false);
// Main at a joined frame boundary. Hosts choose an explicit frame clock (e.g. ready frames).
void UpdateProfilingSession(const FProfilingOptions& InOptions, std::uint64_t InFrame);
} // namespace Hyperion
