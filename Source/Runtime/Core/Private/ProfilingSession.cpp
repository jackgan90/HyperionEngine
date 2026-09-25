#include "Hyperion/Core/ProfilingSession.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace Hyperion
{
std::uint32_t ParseProfilingCategories(std::string_view InNames)
{
	constexpr std::array<std::string_view, 8> Names{"frame", "render", "material", "rhi",
	                                                "tasks", "assets", "detail",   "gpu"};
	std::uint32_t Mask{};
	while (!InNames.empty())
	{
		const auto End = InNames.find(',');
		const auto Name = InNames.substr(0, End);
		const auto Found = std::ranges::find(Names, Name);
		if (Found == Names.end())
		{
			throw std::invalid_argument("Unknown profiling category: " + std::string(Name));
		}
		Mask |= 1U << std::distance(Names.begin(), Found);
		if (End == std::string_view::npos)
		{
			break;
		}
		if (End + 1 == InNames.size())
		{
			throw std::invalid_argument("Profiling categories must not end with a comma");
		}
		InNames.remove_prefix(End + 1);
	}
	if (!Mask)
	{
		throw std::invalid_argument("Provide at least one profiling category");
	}
	return Mask;
}

void InitializeProfilingSession(const FProfilingOptions& InOptions, bool bInDeferStart)
{
	if (!InOptions.Mask && !InOptions.bWait)
	{
		if (InOptions.Start || InOptions.Frames || InOptions.bSampling)
		{
			throw std::invalid_argument("Profiling window requires enabled categories");
		}
		return;
	}
	if (!GetProfilingStatus().bCompiled)
	{
		throw std::runtime_error("Profiling is not compiled in; build with HYP_ENABLE_TRACY=ON");
	}
	SetProfileThreadName("Main");
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (InOptions.bWait && !GetProfilingConnection())
	{
		if (std::chrono::steady_clock::now() >= Deadline)
		{
			throw std::runtime_error("Timed out waiting for Tracy collector");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	if (!InOptions.Start && !bInDeferStart)
	{
		SetProfilingMask(InOptions.Mask);
		SetProfilingSampling(InOptions.bSampling);
	}
}

void UpdateProfilingSession(const FProfilingOptions& InOptions, std::uint64_t InFrame)
{
	if (InOptions.Mask && InFrame == InOptions.Start)
	{
		SetProfilingMask(InOptions.Mask);
		SetProfilingSampling(InOptions.bSampling);
	}
	if (InOptions.Frames && InFrame >= InOptions.Start && InFrame - InOptions.Start == InOptions.Frames)
	{
		SetProfilingMask(0);
	}
}
} // namespace Hyperion
