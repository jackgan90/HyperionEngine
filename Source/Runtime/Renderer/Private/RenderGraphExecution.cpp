#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{
void JoinRecordings(FTaskSystem& InTasks, std::span<const FTaskHandle> InRecordings, std::exception_ptr& OutError)
{
	for (const auto& Task : InRecordings)
	{
		try
		{
			InTasks.Wait(Task);
		}
		catch (...)
		{
			if (!OutError)
			{
				OutError = std::current_exception();
			}
		}
	}
}

struct FFrameRecording
{
	IRHISwapchain* Swapchain{};
	std::vector<FRecordedList> Lists;
	std::vector<std::shared_ptr<const FPassCommands>> Commands;
	std::uint32_t Threads{};

	void RecordPartition(std::uint32_t InThread)
	{
		for (std::size_t Index = InThread; Index < Commands.size(); Index += Threads)
		{
			Lists[Index] = Swapchain->RecordOwned(static_cast<std::uint32_t>(Index), Commands[Index]);
		}
	}
};

std::vector<FRecordedList> RecordFrame(FTaskSystem& InTasks, IRHISwapchain& InSwapchain,
                                       std::vector<FPassCommands> InCommands)
{
	HYP_PERF_SCOPE_C(Rhi, RecordFrame);
	auto Recording = std::make_shared<FFrameRecording>();
	Recording->Swapchain = &InSwapchain;
	Recording->Lists.resize(InCommands.size());
	Recording->Commands.reserve(InCommands.size());
	for (auto& Pass : InCommands)
	{
		Recording->Commands.push_back(std::make_shared<const FPassCommands>(std::move(Pass)));
	}
	Recording->Threads = InSwapchain.GetCapabilities().QueryFeature(ERHIFeature::ConcurrentRecording).bEnabled
	                         ? std::min(InTasks.RhiThreadCount(), static_cast<std::uint32_t>(InCommands.size()))
	                         : 1U;
	std::vector<FTaskHandle> Peers;
	Peers.reserve(Recording->Threads - 1);
	std::exception_ptr Error;
	try
	{
		for (std::uint32_t Thread = 1; Thread < Recording->Threads; ++Thread)
		{
			Peers.push_back(InTasks.Dispatch({EDomain::Rhi, Thread},
			                                 [Recording, Thread]
			                                 {
				                                 Recording->RecordPartition(Thread);
			                                 }));
		}
		Recording->RecordPartition(0);
	}
	catch (...)
	{
		Error = std::current_exception();
	}
	// Peers may still own frame allocators after the coordinator's partition or a dispatch fails.
	JoinRecordings(InTasks, Peers, Error);
	if (Error)
	{
		std::rethrow_exception(Error);
	}
	HYP_PERF_PLOT(Rhi, FrameRecordingTasks, double(Peers.size()));
	return std::move(Recording->Lists);
}

FImage ExecuteFrame(FRenderGraph& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture, const FRenderGraphCallbacks& InCallbacks)
{
	HYP_PERF_SCOPE_C(Rhi, RhiFrame);
	InTasks.Require({EDomain::Rhi, 0});
	bool bAcquired = false;
	try
	{
		if (InCallbacks.BeforePrepare)
		{
			InCallbacks.BeforePrepare();
		}
		auto Commands = InGraph.CompileAndConsume();
		if (Commands.size() > InSwapchain.GetCapabilities().MaxRecordingContexts)
		{
			throw std::runtime_error("Graph exceeds backend recording context capacity");
		}
		if (bInCapture && !InSwapchain.GetCapabilities().QueryFeature(ERHIFeature::Readback).bEnabled)
		{
			throw std::runtime_error("Backend does not enable image readback");
		}
		InSwapchain.BeginFrame(InSize);
		bAcquired = true;
		auto Lists = RecordFrame(InTasks, InSwapchain, std::move(Commands));
		auto Image = InSwapchain.EndFrame(Lists, bInVsync, bInCapture);
		if (InCallbacks.AfterSubmit)
		{
			InCallbacks.AfterSubmit();
		}
		return Image;
	}
	catch (...)
	{
		const auto Error = std::current_exception();
		try
		{
			if (bAcquired)
			{
				InSwapchain.CancelFrame();
			}
		}
		catch (const std::exception& CleanupError)
		{
			Log(ELogLevel::Error, std::string("Frame cancellation failed: ") + CleanupError.what());
		}
		try
		{
			if (InCallbacks.OnFailure)
			{
				InCallbacks.OnFailure();
			}
		}
		catch (...)
		{
			Log(ELogLevel::Error, "Frame callback cleanup failed");
		}
		std::rethrow_exception(Error);
	}
}
} // namespace

FImage ExecuteGraphOnRhi(FRenderGraph InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                         bool bInVsync, bool bInCapture, const FRenderGraphCallbacks& InCallbacks)
{
	return ExecuteFrame(InGraph, InTasks, InSwapchain, InSize, bInVsync, bInCapture, InCallbacks);
}

FImage ExecuteGraph(const FRenderGraph& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture)
{
	auto Copy = InGraph;
	return ExecuteGraph(std::move(Copy), InTasks, InSwapchain, InSize, bInVsync, bInCapture);
}

FImage ExecuteGraph(FRenderGraph&& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture, const std::function<void()>& InAfterSubmit)
{
	return ExecuteGraph(std::move(InGraph), InTasks, InSwapchain, InSize, bInVsync, bInCapture,
	                    FRenderGraphCallbacks{{}, InAfterSubmit, {}});
}

FImage ExecuteGraph(FRenderGraph&& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture, const FRenderGraphCallbacks& InCallbacks)
{
	HYP_PERF_SCOPE_C(Render, ExecuteRenderGraph);
	auto Image = std::make_shared<FImage>();
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [Image, Tasks = &InTasks, Swapchain = &InSwapchain, Size = InSize, bVsync = bInVsync,
	                               bCapture = bInCapture, Callbacks = InCallbacks, Graph = std::move(InGraph)]() mutable
	                              {
		                              *Image = ExecuteGraphOnRhi(std::move(Graph), *Tasks, *Swapchain, Size, bVsync,
		                                                         bCapture, Callbacks);
	                              }));
	return std::move(*Image);
}
} // namespace Hyperion
