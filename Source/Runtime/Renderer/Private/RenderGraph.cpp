#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FAttachmentInitialization
{
	bool bFull{};
	std::vector<FViewport> Regions;

	void Use(const std::optional<FViewport>& InViewport, bool bInInitialize, const char* InName)
	{
		if (bInInitialize)
		{
			if (InViewport)
			{
				Regions.push_back(*InViewport);
			}
			else
			{
				bFull = true;
			}
			return;
		}
		if (bFull)
		{
			return;
		}
		if (InViewport && std::any_of(Regions.begin(), Regions.end(),
		                              [&](const FViewport& InRegion)
		                              {
			                              return InRegion.X <= InViewport->X && InRegion.Y <= InViewport->Y &&
			                                     InRegion.X + InRegion.Width >= InViewport->X + InViewport->Width &&
			                                     InRegion.Y + InRegion.Height >= InViewport->Y + InViewport->Height;
		                              }))
		{
			return;
		}
		throw std::runtime_error(std::string("Graph loads undefined ") + InName);
	}
};

struct FGraphAttachments
{
	FAttachmentInitialization Color;
	FAttachmentInitialization Depth;
	FAttachmentInitialization Stencil;
	std::optional<std::pair<std::uint64_t, ERHIDepthFormat>> DepthOwner;

	void Validate(const FColorPass& InPass)
	{
		const auto& Commands = InPass.Commands;
		if ((Commands.bClearDepth && !Commands.bUseDepth) || (Commands.bClearStencil && !Commands.bUseStencil) ||
		    (Commands.bUseDepth && Commands.DepthFormat == ERHIDepthFormat::None) ||
		    (Commands.bUseStencil && Commands.DepthFormat != ERHIDepthFormat::D32S8))
		{
			throw std::runtime_error("Invalid graph depth/stencil attachment or clear");
		}
		const auto Key = std::make_pair(Commands.DepthDomain, Commands.DepthFormat);
		if ((Commands.bUseDepth || Commands.bUseStencil) && DepthOwner != Key)
		{
			Depth = {};
			Stencil = {};
			DepthOwner = Key;
		}
		if (Commands.bUseDepth)
		{
			Depth.Use(Commands.Viewport, Commands.bClearDepth, "depth");
		}
		if (Commands.bUseStencil)
		{
			Stencil.Use(Commands.Viewport, Commands.bClearStencil, "stencil");
		}
		Color.Use(Commands.Viewport, InPass.Load != EColorLoad::Load, "color contents");
	}
};

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
	HYP_PERF_SCOPE_C(Render, CompileRenderGraph);
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
	FGraphAttachments Attachments;
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
			Attachments.Validate(Pass);
			auto Commands = Pass.Commands;
			Commands.bClear = Pass.Load == EColorLoad::Clear;
			if (Result.empty())
			{
				Commands.TransitionFrom = EResourceState::Present;
				Commands.TransitionTo = EResourceState::RenderTarget;
			}
			Result.push_back(std::move(Commands));
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
	HYP_PERF_SCOPE_C(Render, ExecuteRenderGraph);
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
