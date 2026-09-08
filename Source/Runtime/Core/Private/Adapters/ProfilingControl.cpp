#include "Hyperion/Core/Profiling.h"
#include <mutex>
#include <stdexcept>
#if HYP_ENABLE_PROFILING
#include <Windows.h>
#include <tracy/TracyC.h>
#endif

namespace Hyperion
{
std::atomic<std::uint32_t> ProfileMask{};

namespace
{
std::mutex ControlMutex;
EProfileSampling Sampling{};

void StopSampling()
{
#if HYP_ENABLE_PROFILING
	if (Sampling == EProfileSampling::Requested)
	{
		TracyCEndSamplingProfiling();
	}
#endif
	Sampling = EProfileSampling::Disabled;
}

#if HYP_ENABLE_PROFILING
bool CanRequestSampling()
{
	HANDLE Token{};
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token))
	{
		return false;
	}
	TOKEN_ELEVATION Elevation{};
	DWORD Size{};
	const bool bElevated = GetTokenInformation(Token, TokenElevation, &Elevation, sizeof(Elevation), &Size) &&
	                       Elevation.TokenIsElevated != 0;
	CloseHandle(Token);
	return bElevated;
}
#endif
} // namespace

void SetProfilingMask(std::uint32_t InMask)
{
	if ((InMask & ~ProfileAllMask) != 0)
	{
		throw std::invalid_argument("Unknown profiling category bits");
	}
#if !HYP_ENABLE_PROFILING
	if (InMask)
	{
		throw std::runtime_error("Profiling is not compiled in; configure HYP_ENABLE_TRACY=ON");
	}
#endif
	std::lock_guard Lock(ControlMutex);
	ProfileMask.store(InMask, std::memory_order_relaxed);
	if (!InMask)
	{
		StopSampling();
	}
}

EProfileSampling SetProfilingSampling(bool bInEnabled)
{
	std::lock_guard Lock(ControlMutex);
	if (!bInEnabled)
	{
		StopSampling();
		return Sampling;
	}
	if (Sampling == EProfileSampling::Requested)
	{
		return Sampling;
	}
	Sampling = EProfileSampling::Unavailable;
#if HYP_ENABLE_PROFILING
	if (ProfileMask.load(std::memory_order_relaxed) && CanRequestSampling() && TracyCBeginSamplingProfiling())
	{
		// Tracy's API acknowledges the request, not ETW session success. Verify samples in the capture.
		Sampling = EProfileSampling::Requested;
	}
#endif
	return Sampling;
}

FProfileStatus GetProfilingStatus()
{
#if HYP_ENABLE_PROFILING
	std::lock_guard Lock(ControlMutex);
	return {HYP_ENABLE_PROFILING != 0, GetProfilingConnection() != 0, ProfileMask.load(std::memory_order_relaxed),
	        Sampling};
#else
	return {};
#endif
}
} // namespace Hyperion
