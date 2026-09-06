#pragma once
#include "Hyperion/Tasks/TaskSystem.h"
#include <atomic>
#include <exception>
#include <stdexcept>

namespace Hyperion
{
class FCancellationToken
{
public:
	void Cancel() const
	{
		Cancelled->store(true);
	}

	bool IsCancelled() const
	{
		return Cancelled->load();
	}

	void Check() const
	{
		if (IsCancelled())
		{
			throw std::runtime_error("Request canceled");
		}
	}

private:
	std::shared_ptr<std::atomic_bool> Cancelled = std::make_shared<std::atomic_bool>(false);
};

template<class T> struct TAsyncState
{
	std::shared_ptr<const T> Value;
	std::exception_ptr Error;
};

// Result state is immutable after Completion becomes ready. The task system
// completion mutex provides the publication barrier, including for exceptions.
template<class T> class TAsyncResult
{
public:
	TAsyncResult() = default;

	TAsyncResult(FTaskHandle InCompletion, std::shared_ptr<TAsyncState<T>> InState)
	    : Completion(std::move(InCompletion)), State(std::move(InState))
	{
	}

	bool Ready() const
	{
		return Completion.Ready();
	}

	const FTaskHandle& Task() const
	{
		return Completion;
	}

	std::shared_ptr<const T> GetReady() const
	{
		if (!State || !Ready())
		{
			throw std::logic_error("Result is not ready");
		}
		if (State->Error)
		{
			std::rethrow_exception(State->Error);
		}
		return State->Value;
	}

	std::shared_ptr<const T> Get(FTaskSystem& InTasks) const
	{
		InTasks.Wait(Completion);
		return GetReady();
	}

private:
	FTaskHandle Completion;
	std::shared_ptr<TAsyncState<T>> State;
};

template<class T, class F>
TAsyncResult<T> DispatchAsync(FTaskSystem& InTasks, FTarget InTarget, F InWork, FCancellationToken InCancellation = {})
{
	auto State = std::make_shared<TAsyncState<T>>();
	auto Completion = InTasks.Dispatch(InTarget,
	                                   [State, InWork = std::move(InWork), InCancellation]() mutable
	                                   {
		                                   try
		                                   {
			                                   InCancellation.Check();
			                                   auto Value = std::make_shared<T>(InWork());
			                                   InCancellation.Check();
			                                   State->Value = std::move(Value);
		                                   }
		                                   catch (...)
		                                   {
			                                   State->Error = std::current_exception();
			                                   throw;
		                                   }
	                                   });
	return {std::move(Completion), std::move(State)};
}
} // namespace Hyperion
