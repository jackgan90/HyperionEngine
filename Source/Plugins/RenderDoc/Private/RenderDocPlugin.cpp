#include "Hyperion/RenderDoc/RenderDocPlugin.h"

namespace Hyperion
{
FRenderDocPlugin::FRenderDocPlugin(FFrameCaptureSettings InSettings) : Service(std::move(InSettings))
{
}

void FRenderDocPlugin::Start()
{
	Service.Initialize();
}

void FRenderDocPlugin::Stop() noexcept
{
	Service.Shutdown();
}

FFrameCapture& FRenderDocPlugin::Capture()
{
	return Service;
}

void RegisterRenderDocPlugin(FPluginRegistry& InRegistry, FFrameCaptureSettings InSettings)
{
	InRegistry.Add({"renderdoc",
	                {},
	                [Settings = std::move(InSettings)]
	                {
		                return std::make_unique<FRenderDocPlugin>(Settings);
	                }});
}
} // namespace Hyperion
