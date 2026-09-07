#pragma once
#include "Hyperion/RHI/RHIDevice.h"

namespace Hyperion
{
struct FD3D12DeviceState;

class FD3D12RHIDevice final : public IRHIDevice
{
public:
	explicit FD3D12RHIDevice(const FRHIDeviceDesc& InDesc);
	~FD3D12RHIDevice() override;
	const FRHICapabilities& GetCapabilities() const noexcept override;
	FRHIFeatureSupport QueryFeature(ERHIFeature InFeature) const override;
	FBuffer CreateBuffer(std::span<const std::byte> InBytes) override;
	FBuffer CreateBuffer(const FBufferDesc& InDesc, std::span<const std::byte> InBytes = {}) override;
	FBufferSlice PublishConstantSlice(const FBuffer& InBuffer, std::uint64_t InOffset,
	                                  std::span<const std::byte> InBytes) override;
	void ResetConstantBuffer(const FBuffer& InBuffer) override;
	FSampler CreateSampler(const FSamplerDesc& InDesc) override;
	FResourceBindingLayout CreateBindingLayout(const FResourceBindingLayoutDesc& InDesc) override;
	FResourceBindingSet CreateBindingSet(const FResourceBindingSetDesc& InDesc) override;
	FTexture CreateTexture(const FImage& InImage) override;
	std::vector<FTexture> CreateTexturesAsync(std::span<const FTextureDesc> InTextures) override;
	bool TexturesReady(std::span<const FTexture> InTextures) override;
	FPipeline CreatePipeline(const FPipelineDesc& InDesc) override;
	std::unique_ptr<IRHISwapchain> CreateSwapchain(const FRHISwapchainDesc& InDesc) override;
	void WaitIdle() override;
	void CollectCompletedResources() override;
	FDeviceStats Statistics() const override;

private:
	std::shared_ptr<FD3D12DeviceState> State;
};
} // namespace Hyperion
