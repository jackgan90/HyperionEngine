#pragma once
#include "hyperion/Plugins.h"
#include "hyperion/RHI.h"
#include "hyperion/TaskSystem.h"

namespace Hyperion
{
enum class EColorLoad
{
	Clear,
	Discard,
	Load
};

struct FColorPass
{
	FPassCommands Commands;
	EColorLoad Load = EColorLoad::Load;
	std::vector<std::size_t> After;
};

// Initial graph scope: one imported swapchain color target, initially undefined.
class FRenderGraph
{
public:
	std::size_t Add(FColorPass InPass);
	std::vector<FPassCommands> Compile() const;

private:
	std::vector<FColorPass> Passes;
};

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

// Called from Render; jobs are routed to engine-owned RHI executors.
FImage ExecuteGraph(const FRenderGraph& InGraph, FTaskSystem& InTasks, FRhiDevice& InDevice, FSize InSize, bool InVsync,
                    bool InCapture);
void RegisterTrianglePlugin(FPluginRegistry& InRegistry, FRhiDevice& InDevice, FShaderCompiler& InCompiler,
                            FTaskSystem& InTasks);
} // namespace Hyperion
