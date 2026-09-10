#pragma once
#include "Hyperion/Tasks/TaskSystem.h"

namespace Hyperion
{
struct FFramePipelineLimits
{
	static constexpr std::uint32_t MaximumLead = 16;
	std::uint32_t MainLead = 1;
	std::uint32_t RenderLead = 1;
};

struct FFramePipelineProgress
{
	std::uint64_t Submitted{};
	std::uint64_t RenderCompleted{};
	std::uint64_t RhiCompleted{};
};

struct FFramePipelineState;
struct FFrameTicketState;

// CPU completion only. Ready reports termination; Wait observes errors before reading results.
// Wait runs on Main and requires the task system to remain alive.
class FFrameTicket
{
public:
	FFrameTicket() = default;
	std::uint64_t Frame() const;
	bool Ready() const;
	void Wait() const;

private:
	std::shared_ptr<FFrameTicketState> State;
	friend class FFramePipeline;
};

// Main owns admission and lifetime; Render builds an owned callable executed on RHI 0.
// Callables must not borrow producer stack data. Services must outlive Drain.
// RHI work must not wait on Render, and must join every same-frame recording peer.
// Main callbacks pumped by a wait cannot reenter Submit/Drain.
class FFramePipeline
{
public:
	FFramePipeline(FTaskSystem& InTasks, FFramePipelineLimits InLimits = {});
	~FFramePipeline();
	FFramePipeline(const FFramePipeline&) = delete;
	FFramePipeline& operator=(const FFramePipeline&) = delete;
	FFrameTicket Submit(std::function<std::function<void()>()> InPrepare);
	FFrameTicket Skip();
	// Main; joins every admitted frame even if one failed, then rethrows the first error.
	void Drain();
	FFramePipelineProgress Progress() const;

private:
	std::shared_ptr<FFramePipelineState> State;
};
} // namespace Hyperion
