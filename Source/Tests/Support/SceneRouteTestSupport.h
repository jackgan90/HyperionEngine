#pragma once
#include "Hyperion/Renderer/SceneRenderPipeline.h"

namespace Hyperion
{
struct FSceneRouteCase
{
	std::vector<std::string> Authored;
	std::vector<std::string> DeferredDraws;
	std::vector<std::string> ForwardDraws;
	bool bDeferredPick{};
	bool bForwardPick{};
};

// Literal compatibility oracle, independent from the production route inventory.
inline const std::vector<FSceneRouteCase>& SceneRouteCases()
{
	static const std::vector<FSceneRouteCase> Cases{
	    {{"Forward"}, {"Forward"}, {"Forward"}, true, true},
	    {{"HdrForwardOpaque", "Forward"}, {}, {"HdrForwardOpaque"}, false, true},
	    {{"DeferredBase", "Forward"}, {"DeferredBase"}, {}, true, false},
	    {{"HdrCompatibility", "Forward"}, {"HdrCompatibility"}, {}, true, false},
	    {{"HdrTransparent", "Forward"}, {"HdrTransparent"}, {"HdrTransparent"}, true, true},
	    {{"HdrForwardOpaque", "DeferredBase", "Forward"}, {"DeferredBase"}, {"HdrForwardOpaque"}, true, true},
	    {{"DeferredBase", "HdrCompatibility", "HdrTransparent", "Forward"},
	     {"DeferredBase", "HdrCompatibility", "HdrTransparent"},
	     {"HdrTransparent"},
	     true,
	     true},
	    {{"HdrForwardOpaque", "HdrTransparent", "Forward"},
	     {"HdrTransparent"},
	     {"HdrForwardOpaque", "HdrTransparent"},
	     true,
	     true},
	    {{"ShadowDepth", "Forward"}, {"Forward"}, {"Forward"}, true, true},
	    {{"ShadowDepth"}, {}, {}, false, false},
	    {{"CustomOnly"}, {}, {}, false, false},
	    {{"CustomOnly", "Forward"}, {"Forward"}, {"Forward"}, true, true}};
	return Cases;
}

struct FSceneRouteFixture
{
	FTaskSystem& Tasks;
	FRenderSession& Session;
	FShaderCompiler& Compiler;
	FRenderView& View;
	FForwardPipelineStatistics& Statistics;
	std::function<FImage(ESceneRenderPipeline, std::vector<std::shared_ptr<const FPassCommands>>&)> Frame;
};

void RunSceneRouteTests(FSceneRouteFixture InFixture);
} // namespace Hyperion
