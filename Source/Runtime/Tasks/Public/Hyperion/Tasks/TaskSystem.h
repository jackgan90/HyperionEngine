#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Hyperion
{
enum class EDomain
{
	Main,
	Render,
	Rhi,
	Worker
};

struct FTarget
{
	EDomain Domain = EDomain::Worker;
	std::uint32_t Index = 0;
	bool operator==(const FTarget&) const = default;
};
struct FTaskState;

class FTaskHandle
{
public:
	FTaskHandle() = default;
	bool Ready() const;

	explicit operator bool() const
	{
		return static_cast<bool>(State);
	}

private:
	explicit FTaskHandle(std::shared_ptr<FTaskState> InState) : State(std::move(InState))
	{
	}

	std::shared_ptr<FTaskState> State;
	friend class FTaskSystem;
};

struct FExecutionStats
{
	std::string Name;
	std::uint64_t Executed{};
	std::uint64_t ThreadId{};
};

class FTaskSystem
{
public:
	explicit FTaskSystem(std::uint32_t InWorkers = 0, std::uint32_t InRhiThreads = 2);
	~FTaskSystem();
	FTaskSystem(const FTaskSystem&) = delete;
	FTaskSystem& operator=(const FTaskSystem&) = delete;
	FTaskHandle Dispatch(FTarget InTarget, std::function<void()> InWork,
	                     std::span<const FTaskHandle> InDependencies = {});
	void Wait(const FTaskHandle& InHandle);
	void WaitAll(std::span<const FTaskHandle> InHandles);
	void PumpMain();
	void Shutdown();
	std::vector<FExecutionStats> Statistics() const;
	std::uint32_t RhiThreadCount() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
