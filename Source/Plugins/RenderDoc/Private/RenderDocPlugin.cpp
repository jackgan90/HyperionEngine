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

void FRenderDocPlugin::Start(FPluginContext& InContext)
{
	Start();
	InContext.Provide(Service);
}

FFrameCapture& FRenderDocPlugin::Capture()
{
	return Service;
}

void RegisterRenderDocPlugin(FPluginRegistry& InRegistry, FFrameCaptureSettings InSettings)
{
	FPluginDescriptor Descriptor{"renderdoc",
	                             {},
	                             [Settings = std::move(InSettings)]
	                             {
		                             return std::make_unique<FRenderDocPlugin>(Settings);
	                             }};
	Descriptor.Before = {"window", "graphics"};
	Descriptor.Provides = {typeid(FFrameCapture)};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
