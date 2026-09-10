#include "Hyperion/Renderer/FramePipeline.h"
#include "Hyperion/Core/Profiling.h"
#include <atomic>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FMainFrameOperation
{
	bool& bActive;

	explicit FMainFrameOperation(bool& bInActive) : bActive(bInActive)
	{
		if (bActive)
		{
			throw std::logic_error("Frame pipeline admission and drain cannot be reentered from Main callbacks");
		}
		bActive = true;
	}

	~FMainFrameOperation()
	{
		bActive = false;
	}
};
} // namespace

struct FFrameTicketState
{
	FTaskSystem* Tasks{};
	std::uint64_t Frame{};
	FTaskHandle Render;
	// Published by Render; read only after Render is ready or joined.
	FTaskHandle Rhi;
};

struct FFramePipelineState
{
	FTaskSystem& Tasks;
	const FFramePipelineLimits Limits;
	// Main owns Frames and Submitted; Render owns Recordings.
	std::deque<FFrameTicket> Frames;
	std::deque<FTaskHandle> Recordings;
	std::uint64_t Submitted{};
	bool bMainOperation{};
	std::atomic_uint64_t RenderCompleted{};
	std::atomic_uint64_t RhiCompleted{};
	std::mutex FailureMutex;
	std::exception_ptr Failure;

	FFramePipelineState(FTaskSystem& InTasks, FFramePipelineLimits InLimits) : Tasks(InTasks), Limits(InLimits)
	{
	}

	void CheckFailure()
	{
		std::lock_guard Lock(FailureMutex);
		if (Failure)
		{
			std::rethrow_exception(Failure);
		}
	}

	void Fail()
	{
		std::lock_guard Lock(FailureMutex);
		if (!Failure)
		{
			Failure = std::current_exception();
		}
	}

	void ExecuteRhi(std::uint64_t InFrame, const std::function<void()>& InWork)
	{
		HYP_PERF_SCOPE_NAMED(EProfileCategory::Rhi, "PipelineRhiFrame", FrameScope);
		HYP_PERF_VALUE(FrameScope, InFrame);
		try
		{
			CheckFailure();
			InWork();
			RhiCompleted.store(InFrame, std::memory_order_release);
		}
		catch (...)
		{
			Fail();
			throw;
		}
	}

	void LimitRender()
	{
		while (!Recordings.empty() && (Recordings.size() > Limits.RenderLead || Recordings.front().Ready()))
		{
			const auto Task = Recordings.front();
			Recordings.pop_front();
			Tasks.Wait(Task);
		}
	}

	void PrepareFrame(const std::shared_ptr<FFramePipelineState>& InOwner,
	                  const std::shared_ptr<FFrameTicketState>& InFrame,
	                  const std::function<std::function<void()>()>& InPrepare)
	{
		HYP_PERF_SCOPE_NAMED(EProfileCategory::Render, "PipelineRenderFrame", FrameScope);
		HYP_PERF_VALUE(FrameScope, InFrame->Frame);
		try
		{
			CheckFailure();
			auto Work = InPrepare();
			if (!Work)
			{
				throw std::logic_error("Frame RHI work is empty");
			}
			InFrame->Rhi = Tasks.Dispatch({EDomain::Rhi, 0},
			                              [Owner = InOwner, Id = InFrame->Frame, Work = std::move(Work)]
			                              {
				                              Owner->ExecuteRhi(Id, Work);
			                              });
			Recordings.push_back(InFrame->Rhi);
			LimitRender();
			RenderCompleted.store(InFrame->Frame, std::memory_order_release);
		}
		catch (...)
		{
			Fail();
			throw;
		}
	}
};

std::uint64_t FFrameTicket::Frame() const
{
	return State ? State->Frame : 0;
}

bool FFrameTicket::Ready() const
{
	return !State || (State->Render.Ready() && State->Rhi.Ready());
}

void FFrameTicket::Wait() const
{
	if (!State)
	{
		return;
	}
	State->Tasks->Require({EDomain::Main});
	std::exception_ptr Error;
	try
	{
		State->Tasks->Wait(State->Render);
	}
	catch (...)
	{
		Error = std::current_exception();
	}
	try
	{
		State->Tasks->Wait(State->Rhi);
	}
	catch (...)
	{
		if (!Error)
		{
			Error = std::current_exception();
		}
	}
	if (Error)
	{
		std::rethrow_exception(Error);
	}
}

FFramePipeline::FFramePipeline(FTaskSystem& InTasks, FFramePipelineLimits InLimits)
    : State(std::make_shared<FFramePipelineState>(InTasks, InLimits))
{
	InTasks.Require({EDomain::Main});
	if (InLimits.MainLead > FFramePipelineLimits::MaximumLead ||
	    InLimits.RenderLead > FFramePipelineLimits::MaximumLead)
	{
		throw std::invalid_argument("CPU frame lead exceeds supported capacity");
	}
}

FFramePipeline::~FFramePipeline()
{
	try
	{
		Drain();
	}
	catch (...)
	{
		// Normal shutdown observes the error; unwinding still joins every frame.
	}
}

FFrameTicket FFramePipeline::Submit(std::function<std::function<void()>()> InPrepare)
{
	auto& P = *State;
	P.Tasks.Require({EDomain::Main});
	FMainFrameOperation Operation(P.bMainOperation);
	P.CheckFailure();
	if (P.Submitted == std::numeric_limits<std::uint64_t>::max())
	{
		throw std::overflow_error("CPU frame identity exhausted");
	}
	if (!InPrepare)
	{
		throw std::invalid_argument("Frame preparation is empty");
	}
	while (!P.Frames.empty() && P.Frames.front().Ready())
	{
		try
		{
			P.Frames.front().Wait();
		}
		catch (...)
		{
			P.Fail();
			throw;
		}
		P.Frames.pop_front();
	}
	FFrameTicket Ticket;
	Ticket.State = std::make_shared<FFrameTicketState>();
	Ticket.State->Tasks = &P.Tasks;
	Ticket.State->Frame = P.Submitted + 1;
	// Reserve ownership before dispatch, including allocation/dispatch failure paths.
	P.Frames.push_back(Ticket);
	try
	{
		Ticket.State->Render = P.Tasks.Dispatch({EDomain::Render},
		                                        [Owner = State, Frame = Ticket.State, Prepare = std::move(InPrepare)]
		                                        {
			                                        Owner->PrepareFrame(Owner, Frame, Prepare);
		                                        });
	}
	catch (...)
	{
		P.Frames.pop_back();
		P.Fail();
		throw;
	}
	++P.Submitted;
	if (P.Submitted > P.Limits.MainLead)
	{
		const auto Required = P.Submitted - P.Limits.MainLead;
		for (const auto& Pending : P.Frames)
		{
			if (Pending.Frame() <= Required)
			{
				try
				{
					P.Tasks.Wait(Pending.State->Render);
				}
				catch (...)
				{
					P.Fail();
					throw;
				}
			}
		}
	}
	P.CheckFailure();
	return Ticket;
}

FFrameTicket FFramePipeline::Skip()
{
	return Submit(
	    []
	    {
		    return []
		    {
		    };
	    });
}

void FFramePipeline::Drain()
{
	auto& P = *State;
	P.Tasks.Require({EDomain::Main});
	FMainFrameOperation Operation(P.bMainOperation);
	std::exception_ptr Error;
	for (const auto& Frame : P.Frames)
	{
		try
		{
			Frame.Wait();
		}
		catch (...)
		{
			P.Fail();
			if (!Error)
			{
				Error = std::current_exception();
			}
		}
	}
	P.Frames.clear();
	// Every Render task is joined, so its otherwise Render-owned queue can now be cleared.
	P.Recordings.clear();
	if (Error)
	{
		std::rethrow_exception(Error);
	}
	P.CheckFailure();
}

FFramePipelineProgress FFramePipeline::Progress() const
{
	State->Tasks.Require({EDomain::Main});
	return {State->Submitted, State->RenderCompleted.load(std::memory_order_acquire),
	        State->RhiCompleted.load(std::memory_order_acquire)};
}
} // namespace Hyperion
