#pragma once
#include "Hyperion/RHI/RHISwapchain.h"

namespace Hyperion
{
// A fully initialized, window-independent device. Resource creation and queue
// synchronization are serialized by the RHI coordinator. Payloads retain their
// device state, and submitted resources remain alive until GPU completion.
class IRHIDevice
{
public:
	virtual ~IRHIDevice() = default;
	virtual const FRHICapabilities& GetCapabilities() const noexcept = 0;
	virtual FRHIFeatureSupport QueryFeature(ERHIFeature InFeature) const = 0;
	virtual FBuffer CreateBuffer(std::span<const std::byte> InBytes) = 0;
	virtual FTexture CreateTexture(const FImage& InImage) = 0;
	virtual FPipeline CreatePipeline(const FPipelineDesc& InDesc) = 0;
	virtual std::unique_ptr<IRHISwapchain> CreateSwapchain(const FRHISwapchainDesc& InDesc) = 0;
	virtual void WaitIdle() = 0;
	virtual FDeviceStats Statistics() const = 0;
};
} // namespace Hyperion
