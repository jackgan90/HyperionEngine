#pragma once
#include "Hyperion/Capture/FrameCapture.h"
#include "Hyperion/Plugins/PluginRuntime.h"

namespace Hyperion
{
class FRenderDocPlugin final : public FPlugin
{
public:
	explicit FRenderDocPlugin(FFrameCaptureSettings InSettings);
	void Start() override;
	void Stop() noexcept override;
	FFrameCapture& Capture();

private:
	FFrameCapture Service;
};

void RegisterRenderDocPlugin(FPluginRegistry& InRegistry, FFrameCaptureSettings InSettings);
} // namespace Hyperion
