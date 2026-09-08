#include <Hyperion/Core/Core.h>
#include <Hyperion/Tasks/TaskSystem.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/task.h>
#include <oneapi/tbb/task_arena.h>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Hyperion
{
struct FTaskState
{
	std::mutex Mutex;
	std::condition_variable Changed;
	bool bDone = false;
	std::exception_ptr Error;
	std::vector<std::function<void()>> Continuations;
	FTarget Target;
	const void* Owner{};
#if HYP_ENABLE_PROFILING
	std::uint64_t ProfileId{};
#endif

	void Subscribe(std::function<void()> InCallback)
	{
		{
			std::lock_guard Lock(Mutex);
			if (!bDone)
			{
				Continuations.push_back(std::move(InCallback));
				return;
			}
		}
		InCallback();
	}
};

namespace
{
thread_local const void* ActiveSystem = nullptr;
thread_local FTarget ActiveTarget{EDomain::Worker};

template<class T, class... Args> std::shared_ptr<T> Tracked(Args&&... InArgs)
{
	void* P = Allocate(sizeof(T), alignof(T), EMemoryTag::Tasks);
	T* Object;
	try
	{
		Object = new (P) T(std::forward<Args>(InArgs)...);
	}
	catch (...)
	{
		Deallocate(P);
		throw;
	}
	return std::shared_ptr<T>(Object,
	                          [](T* InValue)
	                          {
		                          InValue->~T();
		                          Deallocate(InValue);
	                          });
}
} // namespace

bool FTaskHandle::Ready() const
{
	if (!State)
	{
		return true;
	}
	std::lock_guard Lock(State->Mutex);
	return State->bDone;
}

struct FTaskSystem::FImpl
{
	struct FQueue
	{
		std::mutex Mutex;
		std::condition_variable Changed;
		std::deque<std::function<void()>> Pending;
		bool bStop = false;
		std::thread Thread;
		std::string Name;
		std::atomic_uint64_t Executed{};
		std::atomic_uint64_t ThreadId{};
	};

	struct FWork
	{
		std::shared_ptr<FTaskState> State;
		std::function<void()> Body;
		std::vector<FTaskHandle> Dependencies;
		std::atomic_size_t Remaining{};
#if HYP_ENABLE_PROFILING
		std::uint64_t QueuedAt{};
#endif
	};

	std::thread::id MainId = std::this_thread::get_id();
	std::uint32_t RhiCount;
	std::uint32_t WorkerCount;
	oneapi::tbb::global_control Control;
	oneapi::tbb::task_arena Arena;
	std::vector<std::unique_ptr<FQueue>> Queues;
	std::atomic_uint64_t WorkerExecuted{};
	std::atomic_size_t Outstanding{};
	std::mutex Admission;
	std::mutex DrainMutex;
	std::condition_variable Drained;
	bool bAccepting = true;
	bool bClosed = false;

	explicit FImpl(std::uint32_t InWorkers, std::uint32_t InRhi)
	    : RhiCount(InRhi), WorkerCount(InWorkers ? InWorkers
	                                             : std::max(1u, std::thread::hardware_concurrency() > InRhi + 2
	                                                                ? std::thread::hardware_concurrency() - InRhi - 2
	                                                                : 1u)),
	      Control(oneapi::tbb::global_control::max_allowed_parallelism, WorkerCount + 1),
	      Arena(static_cast<int>(WorkerCount + 1), 1)
	{
		if (!InRhi || InRhi > 32 || WorkerCount > 256)
		{
			throw std::invalid_argument("Invalid executor thread count");
		}
		Arena.initialize();
		for (std::uint32_t I = 0; I < InRhi + 3; ++I)
		{
			auto Queue = std::make_unique<FQueue>();
			Queue->Name = I == 0 ? "Main" : I == 1 ? "Render" : I == InRhi + 2 ? "IO" : "RHI " + std::to_string(I - 2);
			Queues.push_back(std::move(Queue));
		}
		Queues[0]->ThreadId = std::hash<std::thread::id>{}(MainId);
		try
		{
			for (std::uint32_t I = 1; I < Queues.size(); ++I)
			{
				auto* Q = Queues[I].get();
				Q->Thread = std::thread(
				    [this, Q, I]
				    {
					    ActiveSystem = this;
					    ActiveTarget = I == 1              ? FTarget{EDomain::Render}
					                   : I == RhiCount + 2 ? FTarget{EDomain::Io}
					                                       : FTarget{EDomain::Rhi, I - 2};
					    Q->ThreadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
					    SetProfileThreadName(Q->Name.c_str());
#ifdef _WIN32
					    const std::wstring Name(Q->Name.begin(), Q->Name.end());
					    SetThreadDescription(GetCurrentThread(), Name.c_str());
#endif
					    for (;;)
					    {
						    std::function<void()> Work;
						    {
							    std::unique_lock Lock(Q->Mutex);
							    Q->Changed.wait(Lock,
							                    [&]
							                    {
								                    return Q->bStop || !Q->Pending.empty();
							                    });
							    if (Q->bStop && Q->Pending.empty())
								    break;
							    Work = std::move(Q->Pending.front());
							    Q->Pending.pop_front();
						    }
						    Work();
					    }
				    });
			}
		}
		catch (...)
		{
			StopQueues();
			throw;
		}
	}

	std::size_t QueueIndex(FTarget InTarget) const
	{
		if (InTarget.Domain == EDomain::Rhi)
		{
			if (InTarget.Index >= RhiCount)
			{
				throw std::out_of_range("Invalid RHI executor index");
			}
			return InTarget.Index + 2;
		}
		if (InTarget.Index != 0)
		{
			throw std::out_of_range("Only RHI executors accept an index");
		}
		return InTarget.Domain == EDomain::Main ? 0 : InTarget.Domain == EDomain::Io ? RhiCount + 2 : 1;
	}

	void StopQueues()
	{
		for (auto& Q : Queues)
		{
			{
				std::lock_guard Lock(Q->Mutex);
				Q->bStop = true;
			}
			Q->Changed.notify_all();
		}
		for (auto& Q : Queues)
		{
			if (Q->Thread.joinable())
			{
				Q->Thread.join();
			}
		}
	}

	void Finish(const std::shared_ptr<FTaskState>& InState, std::exception_ptr InError)
	{
		std::vector<std::function<void()>> Callbacks;
		{
			std::lock_guard Lock(InState->Mutex);
			InState->Error = InError;
			InState->bDone = true;
			Callbacks.swap(InState->Continuations);
		}
		InState->Changed.notify_all();
		for (auto& Callback : Callbacks)
		{
			Callback();
		}
		Outstanding.fetch_sub(1);
		Drained.notify_all();
	}

	void Schedule(const std::shared_ptr<FWork>& InWork)
	{
#if HYP_ENABLE_PROFILING
		if (IsProfilingEnabled(EProfileCategory::Tasks))
		{
			InWork->QueuedAt = ClockNanoseconds();
		}
#endif
		auto Execute = [this, InWork]
		{
			HYP_PERF_SCOPE_NAMED(EProfileCategory::Tasks, "TaskExecute", TaskScope);
			HYP_PERF_VALUE(TaskScope, InWork->State->ProfileId);
#if HYP_ENABLE_PROFILING
			if (InWork->QueuedAt && IsProfilingEnabled(EProfileCategory::Tasks))
			{
				ProfilePlot("TaskQueueMilliseconds", double(ClockNanoseconds() - InWork->QueuedAt) / 1e6);
			}
#endif
			std::exception_ptr Error;
			try
			{
				for (const auto& Dependency : InWork->Dependencies)
				{
					if (!Dependency.State)
					{
						continue;
					}
					std::lock_guard Lock(Dependency.State->Mutex);
					if (Dependency.State->Error)
					{
						std::rethrow_exception(Dependency.State->Error);
					}
				}
				InWork->Body();
			}
			catch (...)
			{
				Error = std::current_exception();
			}
			InWork->Body = {};
			if (InWork->State->Target.Domain == EDomain::Worker)
			{
				WorkerExecuted.fetch_add(1);
			}
			else
			{
				Queues[QueueIndex(InWork->State->Target)]->Executed.fetch_add(1);
			}
			Finish(InWork->State, Error);
		};
		try
		{
			if (InWork->State->Target.Domain == EDomain::Worker)
			{
				Arena.enqueue(
				    [this, Execute]
				    {
					    ActiveSystem = this;
					    ActiveTarget = {EDomain::Worker};
#if HYP_ENABLE_PROFILING
					    static thread_local bool bNamed{};
					    if (!bNamed)
					    {
						    SetProfileThreadName("Worker");
						    bNamed = true;
					    }
#endif
					    Execute();
				    });
			}
			else
			{
				auto& Q = *Queues[QueueIndex(InWork->State->Target)];
				{
					std::lock_guard Lock(Q.Mutex);
					Q.Pending.push_back(std::move(Execute));
				}
				Q.Changed.notify_one();
			}
		}
		catch (...)
		{
			Finish(InWork->State, std::current_exception());
		}
	}

	void Pump()
	{
		if (std::this_thread::get_id() != MainId)
		{
			throw std::logic_error("Main queue can only be pumped by its owner");
		}
		auto& Q = *Queues[0];
		for (;;)
		{
			std::function<void()> Work;
			{
				std::lock_guard Lock(Q.Mutex);
				if (Q.Pending.empty())
				{
					break;
				}
				Work = std::move(Q.Pending.front());
				Q.Pending.pop_front();
			}
			ActiveSystem = this;
			ActiveTarget = {EDomain::Main};
			Work();
		}
	}
};

FTaskSystem::FTaskSystem(std::uint32_t InWorkers, std::uint32_t InRhi) : Impl(std::make_unique<FImpl>(InWorkers, InRhi))
{
}

FTaskSystem::~FTaskSystem()
{
	Shutdown();
}

FTaskHandle FTaskSystem::Dispatch(FTarget InTarget, std::function<void()> InBody,
                                  std::span<const FTaskHandle> InDependencies)
{
	auto& S = *Impl;
	if (!InBody)
	{
		throw std::invalid_argument("Empty task");
	}
	S.QueueIndex(InTarget);
	for (const auto& Dependency : InDependencies)
	{
		if (Dependency.State && Dependency.State->Owner != &S)
		{
			throw std::invalid_argument("Foreign task dependency");
		}
	}
	auto Work = Tracked<FImpl::FWork>();
	Work->State = Tracked<FTaskState>();
	Work->State->Owner = &S;
	Work->State->Target = InTarget;
#if HYP_ENABLE_PROFILING
	if (IsProfilingEnabled(EProfileCategory::Tasks))
	{
		static std::atomic_uint64_t NextProfileId{1};
		Work->State->ProfileId = NextProfileId.fetch_add(1, std::memory_order_relaxed);
	}
#endif
	HYP_PERF_SCOPE_NAMED(EProfileCategory::Tasks, "TaskDispatch", DispatchScope);
	HYP_PERF_VALUE(DispatchScope, Work->State->ProfileId);
	Work->Body = std::move(InBody);
	Work->Dependencies.assign(InDependencies.begin(), InDependencies.end());
	Work->Remaining = InDependencies.size() + 1;
	{
		std::lock_guard Lock(S.Admission);
		if (!S.bAccepting)
		{
			throw std::logic_error("Task system is shutting down");
		}
		S.Outstanding.fetch_add(1);
	}
	auto Arrived = [&S, Work]
	{
		if (Work->Remaining.fetch_sub(1) == 1)
		{
			S.Schedule(Work);
		}
	};
	for (const auto& Dependency : InDependencies)
	{
		if (Dependency.State)
		{
			Dependency.State->Subscribe(Arrived);
		}
		else
		{
			Arrived();
		}
	}
	Arrived();
	return FTaskHandle(Work->State);
}

void FTaskSystem::Wait(const FTaskHandle& InHandle)
{
	if (!InHandle.State)
	{
		return;
	}
	auto State = InHandle.State;
	HYP_PERF_SCOPE_NAMED(EProfileCategory::Tasks, "TaskWait", WaitScope);
	HYP_PERF_VALUE(WaitScope, State->ProfileId);
	auto& S = *Impl;
	if (State->Owner != &S)
	{
		throw std::invalid_argument("Foreign task handle");
	}
	if (!InHandle.Ready())
	{
		if (std::this_thread::get_id() == S.MainId)
		{
			while (!InHandle.Ready())
			{
				S.Pump();
				std::unique_lock Lock(State->Mutex);
				State->Changed.wait_for(Lock, std::chrono::milliseconds(1),
				                        [&]
				                        {
					                        return State->bDone;
				                        });
			}
		}
		else if (ActiveSystem == &S && ActiveTarget.Domain == EDomain::Worker)
		{
#if HYP_ENABLE_PROFILING
			FProfileSuspension Suspension;
#endif
			oneapi::tbb::task::suspend(
			    [State](oneapi::tbb::task::suspend_point InPoint)
			    {
				    State->Subscribe(
				        [InPoint]
				        {
					        oneapi::tbb::task::resume(InPoint);
				        });
			    });
		}
		else
		{
			if (ActiveSystem == &S && ActiveTarget == State->Target)
			{
				throw std::logic_error("Cannot synchronously wait for this dedicated executor's pending task");
			}
			std::unique_lock Lock(State->Mutex);
			State->Changed.wait(Lock,
			                    [&]
			                    {
				                    return State->bDone;
			                    });
		}
	}
	std::lock_guard Lock(State->Mutex);
	if (State->Error)
	{
		std::rethrow_exception(State->Error);
	}
}

void FTaskSystem::WaitAll(std::span<const FTaskHandle> InHandles)
{
	for (const auto& Handle : InHandles)
	{
		Wait(Handle);
	}
}

void FTaskSystem::PumpMain()
{
	Impl->Pump();
}

void FTaskSystem::Shutdown()
{
	auto& S = *Impl;
	if (S.bClosed)
	{
		return;
	}
	if (std::this_thread::get_id() != S.MainId)
	{
		throw std::logic_error("Shutdown must run on Main");
	}
	{
		std::lock_guard Lock(S.Admission);
		S.bAccepting = false;
	}
	while (S.Outstanding.load())
	{
		S.Pump();
		std::unique_lock Lock(S.DrainMutex);
		S.Drained.wait_for(Lock, std::chrono::milliseconds(1));
	}
	S.StopQueues();
	S.bClosed = true;
	if (ActiveSystem == &S)
	{
		ActiveSystem = nullptr;
	}
}

std::vector<FExecutionStats> FTaskSystem::Statistics() const
{
	std::vector<FExecutionStats> Result;
	for (const auto& Q : Impl->Queues)
	{
		Result.push_back({Q->Name, Q->Executed.load(), Q->ThreadId.load()});
	}
	Result.push_back({"Workers (" + std::to_string(Impl->WorkerCount) + ")", Impl->WorkerExecuted.load(), 0});
	return Result;
}

std::uint32_t FTaskSystem::RhiThreadCount() const
{
	return Impl->RhiCount;
}

bool FTaskSystem::IsCurrent(FTarget InTarget) const
{
	if (InTarget.Domain == EDomain::Main)
	{
		return InTarget.Index == 0 && std::this_thread::get_id() == Impl->MainId;
	}
	return ActiveSystem == Impl.get() && ActiveTarget == InTarget;
}

void FTaskSystem::Require(FTarget InTarget) const
{
	if (!IsCurrent(InTarget))
	{
		throw std::logic_error("Operation called from the wrong execution domain");
	}
}
} // namespace Hyperion
