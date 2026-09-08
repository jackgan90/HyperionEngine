#include "Hyperion/Core/ProfilingGpu.h"
#include "ProfilingInternal.h"
#if HYP_ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#endif

namespace Hyperion
{
bool InitializeProfileGpu(FProfileGpuContext& InContext, std::uint64_t InGpuTime, float InNanosecondsPerTick)
{
#if HYP_ENABLE_PROFILING
	const auto Connection = GetProfilingConnection();
	if (!Connection)
	{
		return false;
	}
	if (InContext.Connection == Connection)
	{
		return true;
	}
	if (InContext.Id < 0)
	{
		InContext.Id = tracy::NextGpuContextId();
	}
	if (InContext.Id < 0)
	{
		return false;
	}
	InContext.Connection = Connection;
	auto* Item = tracy::Profiler::QueueSerial();
	tracy::MemWrite(&Item->hdr.type, tracy::QueueType::GpuNewContext);
	tracy::MemWrite(&Item->gpuNewContext.cpuTime, tracy::Profiler::GetTime());
	tracy::MemWrite(&Item->gpuNewContext.gpuTime, static_cast<std::int64_t>(InGpuTime));
	tracy::MemWrite(&Item->gpuNewContext.thread, std::uint32_t{0});
	tracy::MemWrite(&Item->gpuNewContext.period, InNanosecondsPerTick);
	tracy::MemWrite(&Item->gpuNewContext.context, static_cast<std::uint8_t>(InContext.Id));
	tracy::MemWrite(&Item->gpuNewContext.flags, tracy::GpuContextCalibration);
	tracy::MemWrite(&Item->gpuNewContext.type, tracy::GpuContextType::Direct3D12);
	tracy::Profiler::QueueSerialFinish();
	return true;
#else
	(void)InContext;
	(void)InGpuTime;
	(void)InNanosecondsPerTick;
	return false;
#endif
}

void CalibrateProfileGpu(const FProfileGpuContext& InContext, std::uint64_t InGpuTime, std::int64_t InCpuDelta)
{
#if HYP_ENABLE_PROFILING
	if (!InContext.Connection || InContext.Connection != GetProfilingConnection() || InCpuDelta <= 0)
	{
		return;
	}
	auto* Item = tracy::Profiler::QueueSerial();
	tracy::MemWrite(&Item->hdr.type, tracy::QueueType::GpuCalibration);
	tracy::MemWrite(&Item->gpuCalibration.gpuTime, static_cast<std::int64_t>(InGpuTime));
	tracy::MemWrite(&Item->gpuCalibration.cpuTime, tracy::Profiler::GetTime());
	tracy::MemWrite(&Item->gpuCalibration.cpuDelta, InCpuDelta);
	tracy::MemWrite(&Item->gpuCalibration.context, static_cast<std::uint8_t>(InContext.Id));
	tracy::Profiler::QueueSerialFinish();
#else
	(void)InContext;
	(void)InGpuTime;
	(void)InCpuDelta;
#endif
}

FProfileGpuSpan BeginProfileGpu()
{
#if HYP_ENABLE_PROFILING
	return {tracy::Profiler::GetTime(), 0, tracy::GetThreadHandle(), GetProfilingConnection()};
#else
	return {};
#endif
}

void EndProfileGpu(FProfileGpuSpan& InSpan)
{
#if HYP_ENABLE_PROFILING
	InSpan.CpuEnd = tracy::Profiler::GetTime();
#else
	(void)InSpan;
#endif
}

void PublishProfileGpu(const FProfileGpuContext& InContext, const FProfileSite& InSite, const FProfileGpuSpan& InSpan,
                       std::uint64_t InBegin, std::uint64_t InEnd)
{
#if HYP_ENABLE_PROFILING
	if (!InSpan.Connection || InSpan.Connection != InContext.Connection ||
	    InSpan.Connection != GetProfilingConnection() || InEnd < InBegin || !InSpan.CpuEnd)
	{
		return;
	}
	const auto Context = static_cast<std::uint8_t>(InContext.Id);
	const auto* Source = ProfileSource(InSite);
	auto* Item = tracy::Profiler::QueueSerial();
	tracy::MemWrite(&Item->hdr.type, tracy::QueueType::GpuZoneBeginSerial);
	tracy::MemWrite(&Item->gpuZoneBegin.cpuTime, InSpan.CpuBegin);
	tracy::MemWrite(&Item->gpuZoneBegin.srcloc, reinterpret_cast<std::uint64_t>(Source));
	tracy::MemWrite(&Item->gpuZoneBegin.thread, InSpan.Thread);
	tracy::MemWrite(&Item->gpuZoneBegin.queryId, std::uint16_t{0});
	tracy::MemWrite(&Item->gpuZoneBegin.context, Context);
	tracy::Profiler::QueueSerialFinish();
	Item = tracy::Profiler::QueueSerial();
	tracy::MemWrite(&Item->hdr.type, tracy::QueueType::GpuZoneEndSerial);
	tracy::MemWrite(&Item->gpuZoneEnd.cpuTime, InSpan.CpuEnd);
	tracy::MemWrite(&Item->gpuZoneEnd.thread, InSpan.Thread);
	tracy::MemWrite(&Item->gpuZoneEnd.queryId, std::uint16_t{1});
	tracy::MemWrite(&Item->gpuZoneEnd.context, Context);
	tracy::Profiler::QueueSerialFinish();
	// Each pair is fully resolved before these presentation IDs are reused. Native slots are separate.
	for (std::uint16_t Index = 0; Index < 2; ++Index)
	{
		Item = tracy::Profiler::QueueSerial();
		tracy::MemWrite(&Item->hdr.type, tracy::QueueType::GpuTime);
		tracy::MemWrite(&Item->gpuTime.gpuTime, static_cast<std::int64_t>(Index ? InEnd : InBegin));
		tracy::MemWrite(&Item->gpuTime.queryId, Index);
		tracy::MemWrite(&Item->gpuTime.context, Context);
		tracy::Profiler::QueueSerialFinish();
	}
#else
	(void)InContext;
	(void)InSite;
	(void)InSpan;
	(void)InBegin;
	(void)InEnd;
#endif
}
} // namespace Hyperion
