#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <set>
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
} // namespace

std::size_t FRenderGraph::Add(FColorPass InPass)
{
	Passes.push_back(std::move(InPass));
	return Passes.size() - 1;
}

std::vector<FPassCommands> FRenderGraph::Compile() const
{
	if (Passes.empty())
	{
		throw std::runtime_error("Graph requires at least one color pass");
	}
	const auto Count = Passes.size();
	std::vector<std::set<std::size_t>> Deps(Count);
	std::set<std::string> Names;
	for (std::size_t I = 0; I < Count; ++I)
	{
		if (Passes[I].Commands.Name.empty() || !Names.insert(Passes[I].Commands.Name).second)
		{
			throw std::runtime_error("Graph pass names must be unique and nonempty");
		}
		if (Passes[I].Commands.TransitionFrom || Passes[I].Commands.TransitionTo)
		{
			throw std::runtime_error("Graph owns resource transitions");
		}
		if (I)
		{
			Deps[I].insert(I - 1); // Every pass writes the single color target: preserve its hazards.
		}
		for (auto Dependency : Passes[I].After)
		{
			if (Dependency >= Count)
			{
				throw std::runtime_error("Unknown graph dependency");
			}
			Deps[I].insert(Dependency);
		}
	}
	std::vector<FPassCommands> Result;
	std::vector<bool> Visited(Count);
	bool bInitialized = false;
	bool bDepthInitialized = false;
	while (Result.size() < Count)
	{
		bool bProgress = false;
		for (std::size_t I = 0; I < Count; ++I)
		{
			if (Visited[I] || !std::all_of(Deps[I].begin(), Deps[I].end(),
			                               [&](auto InD)
			                               {
				                               return Visited[InD];
			                               }))
			{
				continue;
			}
			const auto& Pass = Passes[I];
			if (Pass.Commands.bClearDepth && !Pass.Commands.bUseDepth)
			{
				throw std::runtime_error("Depth clear requires a depth target");
			}
			if (Pass.Commands.bUseDepth)
			{
				if (!bDepthInitialized && !Pass.Commands.bClearDepth)
				{
					throw std::runtime_error("Graph loads undefined depth");
				}
				bDepthInitialized = true;
			}
			if (Pass.Load == EColorLoad::Load && !bInitialized)
			{
				throw std::runtime_error("Graph loads undefined color contents");
			}
			auto Commands = Pass.Commands;
			Commands.bClear = Pass.Load == EColorLoad::Clear;
			if (Result.empty())
			{
				Commands.TransitionFrom = EResourceState::Present;
				Commands.TransitionTo = EResourceState::RenderTarget;
			}
			Result.push_back(std::move(Commands));
			bInitialized = true;
			Visited[I] = true;
			bProgress = true;
		}
		if (!bProgress)
		{
			throw std::runtime_error("Graph dependency cycle");
		}
	}
	FPassCommands Present;
	Present.Name = "Present transition";
	Present.TransitionFrom = EResourceState::RenderTarget;
	Present.TransitionTo = EResourceState::Present;
	Result.push_back(std::move(Present));
	return Result;
}

FImage ExecuteGraph(const FRenderGraph& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture)
{
	auto Commands = InGraph.Compile();
	// Reject unsupported graphs before acquiring a frame or dispatching any recorder.
	if (Commands.size() > InSwapchain.GetCapabilities().MaxRecordingContexts)
	{
		throw std::runtime_error("Graph exceeds backend recording context capacity");
	}
	if (bInCapture && !InSwapchain.GetCapabilities().QueryFeature(ERHIFeature::Readback).bEnabled)
	{
		throw std::runtime_error("Backend does not enable image readback");
	}
	std::vector<FRecordedList> Lists(Commands.size());
	std::vector<FTaskHandle> Recordings;
	Recordings.reserve(Commands.size());
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              InSwapchain.BeginFrame(InSize);
	                              }));
	try
	{
		for (std::size_t I = 0; I < Commands.size(); ++I)
		{
			Recordings.push_back(InTasks.Dispatch(
			    {EDomain::Rhi, InSwapchain.GetCapabilities().QueryFeature(ERHIFeature::ConcurrentRecording).bEnabled
			                       ? static_cast<std::uint32_t>(I % InTasks.RhiThreadCount())
			                       : 0},
			    [&, I]
			    {
				    Lists[I] = InSwapchain.Record(static_cast<std::uint32_t>(I), Commands[I]);
			    }));
		}
		std::exception_ptr Error;
		JoinRecordings(InTasks, Recordings, Error);
		if (Error)
		{
			std::rethrow_exception(Error);
		}
		FImage Image;
		InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
		                              [&]
		                              {
			                              Image = InSwapchain.EndFrame(Lists, bInVsync, bInCapture);
		                              }));
		return Image;
	}
	catch (...)
	{
		auto Error = std::current_exception();
		// Also covers failure while dispatching, before every recorder was admitted.
		JoinRecordings(InTasks, Recordings, Error);
		try
		{
			InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
			                              [&]
			                              {
				                              InSwapchain.CancelFrame();
			                              }));
		}
		catch (const std::exception& CleanupError)
		{
			Log(ELogLevel::Error, std::string("Frame cancellation failed: ") + CleanupError.what());
		}
		std::rethrow_exception(Error);
	}
}
} // namespace Hyperion
