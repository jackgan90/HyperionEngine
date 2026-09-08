#include "Hyperion/Core/Core.h"
#include "ViewerApplication.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace Hyperion
{
namespace
{
std::uint32_t ParseCategories(std::string_view InNames)
{
	constexpr std::array<std::string_view, 8> Names{"frame", "render", "material", "rhi",
	                                                "tasks", "assets", "detail",   "gpu"};
	std::uint32_t Mask{};
	while (!InNames.empty())
	{
		const auto End = InNames.find(',');
		const auto Name = InNames.substr(0, End);
		const auto Found = std::find(Names.begin(), Names.end(), Name);
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
		throw std::invalid_argument("--profile-categories requires at least one category");
	}
	return Mask;
}
} // namespace

bool ParseProfilingOption(FOptions& InOptions, const std::string& InArg, int InArgc, char** InArgv, int& InIndex)
{
	if (InArg == "--profile")
	{
		InOptions.ProfilingMask |= ProfileBasicMask;
	}
	else if (InArg == "--profile-detail")
	{
		InOptions.ProfilingMask |= ProfileBasicMask | ProfileCategoryMask(EProfileCategory::Detail);
	}
	else if (InArg == "--profile-gpu")
	{
		InOptions.ProfilingMask |= ProfileBasicMask | ProfileCategoryMask(EProfileCategory::Gpu);
	}
	else if (InArg == "--profile-sampling")
	{
		InOptions.ProfilingMask |= ProfileBasicMask;
		InOptions.bProfileSampling = true;
	}
	else if (InArg == "--profile-categories" && InIndex + 1 < InArgc)
	{
		InOptions.ProfilingMask |= ParseCategories(InArgv[++InIndex]);
	}
	else if (InArg == "--profile-start" && InIndex + 1 < InArgc)
	{
		InOptions.ProfileStart = std::stoi(InArgv[++InIndex]);
	}
	else if (InArg == "--profile-frames" && InIndex + 1 < InArgc)
	{
		InOptions.ProfileFrames = std::stoi(InArgv[++InIndex]);
	}
	else if (InArg == "--profile-wait")
	{
		InOptions.bProfileWait = true;
	}
	else
	{
		return false;
	}
	return true;
}

void FViewerApplication::InitializeProfiling()
{
	if (!Options.ProfilingMask && !Options.bProfileWait)
	{
		return;
	}
	if (!GetProfilingStatus().bCompiled)
	{
		throw std::runtime_error("Profiling is not compiled in; build with -Tracy or -Preset profile");
	}
	SetProfileThreadName("Main");
	if (Options.bProfileWait)
	{
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		while (!GetProfilingConnection())
		{
			if (std::chrono::steady_clock::now() >= Deadline)
			{
				throw std::runtime_error("Timed out waiting for Tracy collector");
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	if (Options.ProfileStart == 0)
	{
		SetProfilingMask(Options.ProfilingMask);
		if (Options.bProfileSampling)
		{
			SetProfilingSampling(true);
		}
	}
}

void FViewerApplication::UpdateProfiling(int InFrame)
{
	if (Options.ProfilingMask && InFrame == Options.ProfileStart)
	{
		SetProfilingMask(Options.ProfilingMask);
		if (Options.bProfileSampling)
		{
			const auto Status = SetProfilingSampling(true);
			Log(ELogLevel::Info, Status == EProfileSampling::Unavailable
			                         ? "Profiling sampling unavailable (requires elevated ETW access)"
			                         : "Profiling sampling requested; verify samples in Tracy");
		}
	}
	if (Options.ProfileFrames && InFrame >= Options.ProfileStart &&
	    InFrame - Options.ProfileStart == Options.ProfileFrames)
	{
		SetProfilingMask(0);
	}
	Metrics.Profiling = GetProfilingStatus();
}

void FViewerApplication::HandleProfilingActions(const FDebugActions& InActions)
{
	if (InActions.ProfilingMask)
	{
		SetProfilingMask(*InActions.ProfilingMask);
	}
	if (InActions.Sampling)
	{
		SetProfilingSampling(*InActions.Sampling);
	}
}
} // namespace Hyperion
