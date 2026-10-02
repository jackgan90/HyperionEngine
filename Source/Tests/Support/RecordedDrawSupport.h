#pragma once
#include "Hyperion/RHI/RHISwapchain.h"

namespace Hyperion
{
// Observe the exact owned commands submitted by ExecuteGraph, without compiling or recording a second graph.
class FObservedSwapchain final : public IRHISwapchain
{
public:
	FObservedSwapchain(IRHISwapchain& InSwapchain, std::vector<std::shared_ptr<const FPassCommands>>& OutCommands)
	    : Swapchain(InSwapchain), Commands(OutCommands)
	{
	}

	const FRHICapabilities& GetCapabilities() const noexcept override
	{
		return Swapchain.GetCapabilities();
	}

	void BeginFrame(FSize InSize) override
	{
		Swapchain.BeginFrame(InSize);
	}

	void PrepareFrameRecording(std::uint32_t InCount) override
	{
		Commands.clear();
		Commands.resize(InCount);
		Swapchain.PrepareFrameRecording(InCount);
	}

	FRecordedList Record(std::uint32_t InContext, const FPassCommands& InCommands) override
	{
		return Swapchain.Record(InContext, InCommands);
	}

	FRecordedList RecordOwned(std::uint32_t InContext, std::shared_ptr<const FPassCommands> InCommands) override
	{
		return Swapchain.RecordOwned(InContext, std::move(InCommands));
	}

	FRecordedList RecordBatchOwned(std::uint32_t InContext,
	                               std::span<const std::shared_ptr<const FPassCommands>> InCommands,
	                               std::uint32_t InFirstPass) override
	{
		// ExecuteGraph assigns disjoint ordinal ranges; capacity is fixed before recorder tasks start.
		for (std::size_t Index = 0; Index < InCommands.size(); ++Index)
		{
			Commands.at(InFirstPass + Index) = InCommands[Index];
		}
		return Swapchain.RecordBatchOwned(InContext, InCommands, InFirstPass);
	}

	FImage EndFrame(std::span<const FRecordedList> InLists, bool bInVsync, bool bInCapture) override
	{
		return Swapchain.EndFrame(InLists, bInVsync, bInCapture);
	}

	void CancelFrame() override
	{
		Swapchain.CancelFrame();
	}

	void WaitIdle() override
	{
		Swapchain.WaitIdle();
	}

	void SetGpuTimingEnabled(bool bInEnabled) override
	{
		Swapchain.SetGpuTimingEnabled(bInEnabled);
	}

private:
	IRHISwapchain& Swapchain;
	std::vector<std::shared_ptr<const FPassCommands>>& Commands;
};
} // namespace Hyperion
