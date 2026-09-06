#pragma once
#include "Hyperion/Renderer/RenderPlugin.h"

namespace Hyperion
{
void RegisterTrianglePlugin(FPluginRegistry& InRegistry, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                            FTaskSystem& InTasks);
}
