#pragma once
#include "Hyperion/RHI/RHISwapchain.h"

namespace Hyperion
{
struct FD3D12DeviceState;

class FD3D12RHISwapchain final : public IRHISwapchain
{
public:
	FD3D12RHISwapchain(std::shared_ptr<FD3D12DeviceState> InState, const FRHISwapchainDesc& InDesc);
	~FD3D12RHISwapchain() override;
	const FRHICapabilities& GetCapabilities() const noexcept override;
	void BeginFrame(FSize InSize) override;
	FRecordedList Record(std::uint32_t InContext, const FPassCommands& InCommands) override;
	FImage EndFrame(std::span<const FRecordedList> InLists, bool bInVsync, bool bInCapture) override;
	void CancelFrame() override;
	void WaitIdle() override;
	void SetGpuTimingEnabled(bool bInEnabled) override;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
