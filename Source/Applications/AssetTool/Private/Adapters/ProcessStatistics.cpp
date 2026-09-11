#include "ProcessStatistics.h"
#include <stdexcept>
#include <windows.h>

#include <psapi.h>

namespace Hyperion
{
std::uint64_t AssetToolPeakResidentBytes()
{
	PROCESS_MEMORY_COUNTERS Counters{};
	Counters.cb = sizeof(Counters);
	if (!GetProcessMemoryInfo(GetCurrentProcess(), &Counters, sizeof(Counters)))
	{
		throw std::runtime_error("Cannot query AssetTool process memory");
	}
	return static_cast<std::uint64_t>(Counters.PeakWorkingSetSize);
}
} // namespace Hyperion
