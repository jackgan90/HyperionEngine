#pragma once
#include "Hyperion/RHI/RHISwapchain.h"
#include <stdexcept>

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

	virtual FBuffer CreateBuffer(const FBufferDesc&, std::span<const std::byte> = {})
	{
		throw std::runtime_error("Backend does not support typed buffers");
	}

	virtual FBufferSlice PublishConstantSlice(const FBuffer&, std::uint64_t, std::span<const std::byte>)
	{
		throw std::runtime_error("Backend does not support constant slices");
	}

	virtual void ResetConstantBuffer(const FBuffer&)
	{
		throw std::runtime_error("Backend does not support constant page reuse");
	}

	virtual FSampler CreateSampler(const FSamplerDesc&)
	{
		throw std::runtime_error("Backend does not support independent samplers");
	}

	virtual FResourceBindingLayout CreateBindingLayout(const FResourceBindingLayoutDesc&)
	{
		throw std::runtime_error("Backend does not support binding layouts");
	}

	virtual FResourceBindingSet CreateBindingSet(const FResourceBindingSetDesc&)
	{
		throw std::runtime_error("Backend does not support binding sets");
	}

	virtual FTexture CreateTexture(const FImage& InImage) = 0;

	// Initialized on the graphics queue; starts and finishes graph execution in ShaderRead state.
	virtual FTexture CreateDepthTexture(const FDepthTextureDesc&)
	{
		throw std::runtime_error("Backend does not support sampled depth targets");
	}

	// Initialized on the graphics queue; graph boundaries use ShaderRead.
	virtual FTexture CreateColorTexture(const FColorTextureDesc&)
	{
		throw std::runtime_error("Backend does not support sampled color targets");
	}

	virtual std::vector<FTexture> CreateTexturesAsync(std::span<const FTextureDesc>)
	{
		throw std::runtime_error("Backend does not support asynchronous texture uploads");
	}

	virtual bool TexturesReady(std::span<const FTexture>)
	{
		throw std::runtime_error("Backend does not support upload completion");
	}

	virtual FPipeline CreatePipeline(const FPipelineDesc& InDesc) = 0;
	virtual std::unique_ptr<IRHISwapchain> CreateSwapchain(const FRHISwapchainDesc& InDesc) = 0;
	virtual void WaitIdle() = 0;

	// RHI coordinator: release completed upload/submission retention without an idle wait.
	virtual void CollectCompletedResources()
	{
	}

	virtual FDeviceStats Statistics() const = 0;

	// RHI coordinator. Opt-in bounded collection of completed submissions; neither operation waits.
	// Frame IDs are local to each Swapchain. End after normal fence completion to include the final frame.
	// Membership is fixed at submission; late completion from an earlier capture is never included in a new one.
	virtual void BeginGpuTimingCapture(std::size_t)
	{
		throw std::runtime_error("Backend does not support GPU timing capture");
	}

	virtual FGpuTimingCapture EndGpuTimingCapture()
	{
		return {};
	}
};
} // namespace Hyperion
