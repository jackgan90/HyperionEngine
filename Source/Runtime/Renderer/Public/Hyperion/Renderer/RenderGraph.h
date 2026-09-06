#pragma once
#include "Hyperion/RHI/RHIDevice.h"
#include "Hyperion/Tasks/TaskSystem.h"

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

// Called from Render; jobs are routed to engine-owned RHI executors.
FImage ExecuteGraph(const FRenderGraph& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture);
} // namespace Hyperion
