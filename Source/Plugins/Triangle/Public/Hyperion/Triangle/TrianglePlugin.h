#pragma once
#include "Hyperion/Renderer/RenderPlugin.h"

namespace Hyperion
{
void RegisterTrianglePlugin(FPluginRegistry& InRegistry);
class FRenderSession;
void RegisterTrianglePlugin(FPluginRegistry& InRegistry, FRenderSession& InSession, IRHIDevice& InDevice,
                            FShaderCompiler& InCompiler, FTaskSystem& InTasks);
} // namespace Hyperion
