#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <iostream>
#include <thread>

using namespace Hyperion;

namespace
{
__declspec(noinline) std::uint64_t PlainProbe(std::uint64_t InValue)
{
	return std::rotl(InValue, 7) ^ 0x9e3779b97f4a7c15ULL;
}

__declspec(noinline) std::uint64_t ProfileProbe(std::uint64_t InValue)
{
	HYP_PERF_SCOPE(MicrobenchmarkScope);
	return std::rotl(InValue, 7) ^ 0x9e3779b97f4a7c15ULL;
}

double Measure(bool bInInstrumented)
{
	constexpr std::size_t Count = 200000;
	std::array<double, 7> Samples;
	std::uint64_t Value = 1;
	for (auto& Sample : Samples)
	{
		const auto Start = ClockNanoseconds();
		if (bInInstrumented)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Value = ProfileProbe(Value);
			}
		}
		else
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Value = PlainProbe(Value);
			}
		}
		Sample = double(ClockNanoseconds() - Start) / Count;
	}
	std::cout << "checksum=" << Value << '\n';
	std::sort(Samples.begin(), Samples.end());
	return Samples[Samples.size() / 2];
}
} // namespace

void RunProfileBenchmark()
{
	SetProfilingMask(0);
	const double Baseline = Measure(false);
	const double Disabled = Measure(true);
	std::cout << "compiled=" << HYP_ENABLE_PROFILING << "; baseline_ns=" << Baseline << "; disabled_ns=" << Disabled
	          << "; disabled_delta_ns=" << Disabled - Baseline << '\n';
#if HYP_ENABLE_PROFILING
	SetProfilingMask(ProfileBasicMask);
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (!GetProfilingConnection() && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	const bool bConnected = GetProfilingConnection() != 0;
	const double Enabled = Measure(true);
	std::cout << "connected=" << bConnected << "; enabled_ns=" << Enabled << "; enabled_delta_ns=" << Enabled - Baseline
	          << '\n';
	SetProfilingMask(0);
#endif
}
