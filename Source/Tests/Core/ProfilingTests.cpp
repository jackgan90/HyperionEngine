#include "Hyperion/Core/Core.h"
#include "Hyperion/Tasks/TaskSystem.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace Hyperion;

void RunProfileBenchmark();

namespace
{
void CheckDisabledExpressions()
{
	SetProfilingMask(0);
	int SideEffects{};
	HYP_PERF_PLOT(Frame, DisabledPlot, ++SideEffects);
	HYP_PERF_SCOPE_NAMED(EProfileCategory::Frame, "DisabledScope", Scope);
	HYP_PERF_VALUE(Scope, ++SideEffects);
	HYP_CHECK(SideEffects == 0);
	HYP_CHECK(GetProfilingStatus().Mask == 0);
	HYP_CHECK(SetProfilingSampling(true) == EProfileSampling::Unavailable);
	HYP_CHECK(SetProfilingSampling(false) == EProfileSampling::Disabled);
	bool bInvalidMask{};
	try
	{
		SetProfilingMask(ProfileAllMask + 1);
	}
	catch (const std::invalid_argument&)
	{
		bInvalidMask = true;
	}
	HYP_CHECK(bInvalidMask && GetProfilingStatus().Mask == 0);
#if HYP_ENABLE_PROFILING
	SetProfilingMask(ProfileCategoryMask(EProfileCategory::Frame));
	HYP_PERF_PLOT(Detail, DisabledDetailPlot, ++SideEffects);
	HYP_CHECK(SideEffects == 0);
	SetProfilingMask(0);
#else
	bool bRejected{};
	try
	{
		SetProfilingMask(ProfileBasicMask);
	}
	catch (const std::runtime_error&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
#endif
}

void TraceLeaf()
{
	HYP_PERF_FUNCTION();
	std::this_thread::sleep_for(std::chrono::microseconds(50));
}

void TraceIteration(FTaskSystem& InTasks)
{
	HYP_PERF_SCOPE(TraceOuter);
	TraceLeaf();
	// Multiple declarations in one block must have distinct macro-local identifiers.
	HYP_PERF_SCOPE(TraceSecond);
	InTasks.Wait(InTasks.Dispatch({EDomain::Worker},
	                              [&]
	                              {
		                              HYP_PERF_SCOPE(WorkerParent);
		                              InTasks.Wait(InTasks.Dispatch({EDomain::Worker}, TraceLeaf));
		                              try
		                              {
			                              InTasks.Wait(InTasks.Dispatch({EDomain::Worker},
			                                                            []
			                                                            {
				                                                            HYP_PERF_SCOPE(TraceTaskError);
				                                                            throw std::runtime_error(
				                                                                "Expected task failure");
			                                                            }));
		                              }
		                              catch (const std::runtime_error&)
		                              {
			                              HYP_PERF_SCOPE(TraceTaskErrorRecovery);
		                              }
		                              TraceLeaf();
	                              }));
	try
	{
		HYP_PERF_SCOPE(TraceException);
		throw std::runtime_error("Expected profiling unwind");
	}
	catch (const std::runtime_error&)
	{
		HYP_PERF_SCOPE(TraceRecovery);
	}
	{
		HYP_PERF_SCOPE(TraceGateEnd);
		SetProfilingMask(0);
	}
	SetProfilingMask(ProfileBasicMask);
	HYP_PERF_PLOT(Material, TraceWorkItems, 1);
	ProfileFrame();
}

void CaptureWorkload()
{
	SetProfilingMask(ProfileBasicMask);
	SetProfileThreadName("Profile acceptance Main");
	FTaskSystem Tasks(1, 2);
	const auto ConnectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (!GetProfilingConnection() && std::chrono::steady_clock::now() < ConnectDeadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	HYP_CHECK(GetProfilingConnection() != 0);
	const auto Suspended =
	    Tasks.Dispatch({EDomain::Worker},
	                   [&]
	                   {
		                   HYP_PERF_SCOPE(TraceSuspendedWorker);
		                   Tasks.Wait(Tasks.Dispatch({EDomain::Io},
		                                             []
		                                             {
			                                             std::this_thread::sleep_for(std::chrono::seconds(6));
		                                             }));
		                   TraceLeaf();
	                   });
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
	while (std::chrono::steady_clock::now() < Deadline)
	{
		TraceIteration(Tasks);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	Tasks.Wait(Suspended);
	Tasks.Shutdown();
	SetProfilingMask(0);
}
} // namespace

int main(int InArgc, char** InArgv)
{
	try
	{
		CheckDisabledExpressions();
		if (InArgc == 2 && std::string_view(InArgv[1]) == "--capture-workload")
		{
			CaptureWorkload();
		}
		else if (InArgc == 2 && std::string_view(InArgv[1]) == "--microbenchmark")
		{
			RunProfileBenchmark();
		}
		std::cout << "Profiling contracts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
