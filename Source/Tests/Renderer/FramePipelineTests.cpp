#include "Hyperion/Renderer/FramePipeline.h"
#include "Support/TestSupport.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

using namespace Hyperion;

namespace
{
struct FGate
{
	std::mutex Mutex;
	std::condition_variable Changed;
	bool bOpen{};
	std::atomic<bool> bEntered{};

	void Wait()
	{
		std::unique_lock Lock(Mutex);
		bEntered = true;
		if (!Changed.wait_for(Lock, std::chrono::seconds(5),
		                      [this]
		                      {
			                      return bOpen;
		                      }))
		{
			throw std::runtime_error("Frame test gate timed out");
		}
	}

	void Open()
	{
		std::lock_guard Lock(Mutex);
		bOpen = true;
		Changed.notify_all();
	}
};

struct FRunState
{
	FGate Gate;
	std::atomic_uint32_t MainEntered{};
	std::atomic_uint32_t RenderEntered{};
	std::atomic_uint32_t RhiEntered{};
	std::vector<std::uint32_t> Observed;
};

void Await(const std::function<bool()>& InPredicate)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (!InPredicate())
	{
		HYP_CHECK(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

// Always release the gate before the future joins, including assertion failures.
struct FReleaseGate
{
	FGate& Gate;

	~FReleaseGate()
	{
		Gate.Open();
	}
};

FFramePipelineProgress RunBlocked(const std::shared_ptr<FRunState>& InState, FFramePipelineLimits InLimits,
                                  bool bInBlockRender, bool bInFail)
{
	FTaskSystem Tasks(1, 4);
	FFramePipeline Pipeline(Tasks, InLimits);
	try
	{
		for (std::uint32_t Frame = 1; Frame <= 40; ++Frame)
		{
			InState->MainEntered = Frame;
			Pipeline.Submit(
			    [State = InState, Tasks = &Tasks, Frame, bInBlockRender, bInFail]
			    {
				    Tasks->Require({EDomain::Render});
				    State->RenderEntered = Frame;
				    if (Frame == 1 && bInBlockRender)
				    {
					    State->Gate.Wait();
					    if (bInFail)
					    {
						    throw std::runtime_error("Injected Render failure");
					    }
				    }
				    return [State, Tasks, Frame, bInBlockRender, bInFail]
				    {
					    Tasks->Require({EDomain::Rhi, 0});
					    State->RhiEntered = Frame;
					    if (Frame == 1 && !bInBlockRender)
					    {
						    // Delay the last RHI executor; two other executors receive no work.
						    const auto Peer = Tasks->Dispatch({EDomain::Rhi, 3},
						                                      [State, bInFail]
						                                      {
							                                      State->Gate.Wait();
							                                      if (bInFail)
							                                      {
								                                      throw std::runtime_error("Injected peer failure");
							                                      }
						                                      });
						    Tasks->Wait(Peer);
					    }
					    State->Observed.push_back(Frame);
				    };
			    });
		}
		Pipeline.Drain();
		HYP_CHECK(!bInFail);
	}
	catch (const std::exception&)
	{
		if (!bInFail)
		{
			throw;
		}
		bool bDrainFailed{};
		try
		{
			Pipeline.Drain();
		}
		catch (const std::exception&)
		{
			bDrainFailed = true;
		}
		HYP_CHECK(bDrainFailed && InState->Observed.empty());
		bool bAdmissionFailed{};
		try
		{
			Pipeline.Skip();
		}
		catch (const std::exception&)
		{
			bAdmissionFailed = true;
		}
		HYP_CHECK(bAdmissionFailed);
	}
	return Pipeline.Progress();
}

void CheckLimit(FFramePipelineLimits InLimits, bool bInBlockRender, bool bInFail = false)
{
	auto State = std::make_shared<FRunState>();
	auto Run = std::async(std::launch::async,
	                      [State, InLimits, bInBlockRender, bInFail]
	                      {
		                      return RunBlocked(State, InLimits, bInBlockRender, bInFail);
	                      });
	FReleaseGate Release{State->Gate};
	const auto RenderBoundary = bInBlockRender ? 1U : InLimits.RenderLead + 1;
	const auto MainBoundary = RenderBoundary + InLimits.MainLead;
	Await(
	    [State, RenderBoundary, MainBoundary]
	    {
		    return State->Gate.bEntered && State->RenderEntered >= RenderBoundary && State->MainEntered >= MainBoundary;
	    });
	// The gate controls correctness; this interval only gives incorrectly unbounded producers time to run.
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	HYP_CHECK(State->RenderEntered == RenderBoundary && State->MainEntered == MainBoundary);
	HYP_CHECK(State->RhiEntered == (bInBlockRender ? 0U : 1U));
	State->Gate.Open();
	const auto Progress = Run.get();
	if (!bInFail)
	{
		HYP_CHECK(Progress.Submitted == 40 && Progress.RenderCompleted == 40 && Progress.RhiCompleted == 40);
		HYP_CHECK(State->Observed.size() == 40);
		for (std::uint32_t Index = 0; Index < 40; ++Index)
		{
			HYP_CHECK(State->Observed[Index] == Index + 1);
		}
	}
}

void CheckDrainAndOwnership()
{
	FTaskSystem Tasks(1, 1);
	std::vector<std::uint32_t> Observed;
	std::weak_ptr<const int> Lifetime;
	{
		FFramePipeline Pipeline(Tasks, {2, 2});
		auto Value = std::make_shared<const int>(42);
		Lifetime = Value;
		const auto Ticket = Pipeline.Submit(
		    [Value, Output = &Observed]
		    {
			    return [Value, Output]
			    {
				    Output->push_back(*Value);
			    };
		    });
		Value.reset();
		HYP_CHECK(Ticket.Frame() == 1);
		Ticket.Wait();
		HYP_CHECK(Ticket.Ready() && Observed == std::vector<std::uint32_t>{42});
		Pipeline.Skip();
		Pipeline.Drain();
		Pipeline.Drain();
		HYP_CHECK(Pipeline.Skip().Frame() == 3);
		// Destruction drains the skipped tail while services still exist.
	}
	HYP_CHECK(Lifetime.expired());
	FFrameTicket{}.Wait();
	HYP_CHECK(FFrameTicket{}.Ready());
}

void CheckMainPumping()
{
	FTaskSystem Tasks(1, 1);
	FFramePipeline Pipeline(Tasks, {0, 0});
	bool bPumped{};
	Pipeline.Submit(
	    [Tasks = &Tasks, Pumped = &bPumped]
	    {
		    return [Tasks, Pumped]
		    {
			    Tasks->Wait(Tasks->Dispatch({EDomain::Main},
			                                [Pumped]
			                                {
				                                *Pumped = true;
			                                }));
		    };
	    });
	HYP_CHECK(bPumped);
}

void CheckReentrantAdmission()
{
	FTaskSystem Tasks(1, 1);
	FFramePipeline Pipeline(Tasks, {0, 0});
	bool bRejected{};
	Pipeline.Submit(
	    [Tasks = &Tasks, Pipeline = &Pipeline, Rejected = &bRejected]
	    {
		    return [Tasks, Pipeline, Rejected]
		    {
			    Tasks->Wait(Tasks->Dispatch({EDomain::Main},
			                                [Pipeline, Rejected]
			                                {
				                                try
				                                {
					                                Pipeline->Skip();
				                                }
				                                catch (const std::logic_error&)
				                                {
					                                *Rejected = true;
				                                }
			                                }));
		    };
	    });
	HYP_CHECK(bRejected && Pipeline.Progress().Submitted == 1);
	HYP_CHECK(Pipeline.Skip().Frame() == 2);
}

} // namespace

int main()
{
	try
	{
		for (const auto Limits : {FFramePipelineLimits{0, 0}, {1, 0}, {0, 1}, {1, 1}, {3, 1}, {1, 3}, {16, 16}})
		{
			CheckLimit(Limits, true);
			CheckLimit(Limits, false);
		}
		CheckLimit({3, 3}, true, true);
		CheckLimit({3, 3}, false, true);
		CheckDrainAndOwnership();
		CheckMainPumping();
		CheckReentrantAdmission();
		std::cout << "Bounded CPU frame pipeline passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
