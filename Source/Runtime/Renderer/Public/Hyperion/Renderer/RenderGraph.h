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

struct FGraphicsPass
{
	FPassCommands Commands;
	EColorLoad Load = EColorLoad::Load;
	std::vector<std::size_t> After;
};

using FColorPass = FGraphicsPass; // Compatibility for existing color-only clients.

// One imported swapchain color target plus explicitly written/sampled depth textures.
class FRenderGraph
{
public:
	std::size_t Add(FColorPass InPass);
	std::vector<FPassCommands> Compile() const;
	void ImportDepth(FTexture InTexture); // Previously initialized ShaderRead resource.

private:
	std::vector<FColorPass> Passes;
	std::vector<FTexture> ImportedDepth;
};

// Called from Render; jobs are routed to engine-owned RHI executors.
FImage ExecuteGraph(const FRenderGraph& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture);
} // namespace Hyperion
