#pragma once
#include "Hyperion/Core/Profiling.h"

namespace Hyperion
{
// Native backends own synchronization and query storage; Core only serializes completed spans.
struct FProfileGpuContext
{
	int Id = -1;
	std::uint64_t Connection{};
};

struct FProfileGpuSpan
{
	std::int64_t CpuBegin{};
	std::int64_t CpuEnd{};
	std::uint32_t Thread{};
	std::uint64_t Connection{};
};

bool InitializeProfileGpu(FProfileGpuContext& InContext, std::uint64_t InGpuTime, float InNanosecondsPerTick);
void CalibrateProfileGpu(const FProfileGpuContext& InContext, std::uint64_t InGpuTime, std::int64_t InCpuDelta);
FProfileGpuSpan BeginProfileGpu();
void EndProfileGpu(FProfileGpuSpan& InSpan);
void PublishProfileGpu(const FProfileGpuContext& InContext, const FProfileSite& InSite, const FProfileGpuSpan& InSpan,
                       std::uint64_t InBegin, std::uint64_t InEnd);
} // namespace Hyperion
