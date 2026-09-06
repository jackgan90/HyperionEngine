#pragma once
#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Plugins/PluginRuntime.h"
#include "Hyperion/Renderer/RenderGraph.h"

namespace Hyperion
{
struct FRenderFrame
{
	FSize Size;
	const FAppSettings& Settings;
};

class IRenderPlugin : public FPlugin
{
public:
	virtual void Build(FRenderGraph& InGraph, const FRenderFrame& InFrame) = 0;
};

} // namespace Hyperion
