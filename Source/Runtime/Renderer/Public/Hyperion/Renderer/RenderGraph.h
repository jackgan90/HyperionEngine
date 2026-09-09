#pragma once
#include "Hyperion/RHI/RHIDevice.h"
#include "Hyperion/Tasks/TaskSystem.h"
#include <functional>

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
	// Expansion runs during compilation (RHI 0 in ExecuteGraph). Captures must own frozen frame data.
	// InAfter uses graph entry indices; returned pass dependencies use indices local to that expansion.
	std::size_t AddDeferred(std::function<std::vector<FColorPass>()> InPrepare, std::vector<std::size_t> InAfter = {});
	std::vector<FPassCommands> Compile() const;
	// Consumes the graph's packets; on failure the graph remains valid but may be partially consumed.
	std::vector<FPassCommands> CompileAndConsume();
	void ImportDepth(FTexture InTexture); // Previously initialized ShaderRead resource.

private:
	std::vector<FColorPass> Passes;
	std::vector<FTexture> ImportedDepth;
	std::vector<std::pair<std::size_t, std::function<std::vector<FColorPass>()>>> Preparations;
	void ExpandPreparations();
};

struct FRenderGraphCallbacks
{
	std::function<void()> BeforePrepare;
	std::function<void()> AfterSubmit;
	std::function<void()> OnFailure;
};

// Called from Render; jobs are routed to engine-owned RHI executors.
FImage ExecuteGraph(const FRenderGraph& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture);
// Consuming execution can collect coordinator-only statistics in the successful EndFrame task.
FImage ExecuteGraph(FRenderGraph&& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture, const std::function<void()>& InAfterSubmit = {});
FImage ExecuteGraph(FRenderGraph&& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture, const FRenderGraphCallbacks& InCallbacks);
} // namespace Hyperion
