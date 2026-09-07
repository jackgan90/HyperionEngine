#pragma once
#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Plugins/PluginRuntime.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
struct FRenderFrame
{
	FSize Size;
	FAppSettings Settings;
	FRenderView View;
};

// Scene producers run Start, Update and Stop on Main. The session renders their primitives.
class IScenePlugin : public FPlugin
{
public:
	virtual void Update(FRenderFrame& InFrame) = 0;
};

// Non-scene passes are built on Render; Start and Stop remain on Main.
class IRenderPlugin : public FPlugin
{
public:
	virtual void Build(FRenderGraph& InGraph, const FRenderFrame& InFrame) = 0;
};

} // namespace Hyperion
