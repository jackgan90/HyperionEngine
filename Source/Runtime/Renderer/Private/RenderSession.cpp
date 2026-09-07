#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
FRenderSession::FRenderSession(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler)
    : Tasks(InTasks), Resources(InTasks, InDevice, InCompiler), Scene(InTasks)
{
}

FRenderSession::~FRenderSession()
{
	Close();
}

FRenderSceneClient& FRenderSession::GetScene()
{
	return Scene;
}

FRenderResourceService& FRenderSession::GetResources()
{
	return Resources;
}

std::size_t FRenderSession::Build(FRenderGraph& InGraph, FRenderView InView)
{
	Tasks.Require({EDomain::Render});
	auto Snapshot = PrepareSceneSnapshot(Scene.Collect(InView));
	const auto Count = Snapshot.Items.size();
	LastStatistics = Snapshot.Statistics;
	auto Result = DispatchAsync<std::vector<FColorPass>>(Tasks, {EDomain::Rhi, 0},
	                                                     [ResourceService = &Resources, Frame = std::move(Snapshot)]
	                                                     {
		                                                     return ResourceService->BuildPasses(Frame);
	                                                     });
	Tasks.Wait(Result.Task());
	for (auto Pass : *Result.GetReady())
	{
		LastStatistics.Draws += Pass.Commands.Draws.size();
		InGraph.Add(std::move(Pass));
	}
	return Count;
}

FSceneVisibilityStats FRenderSession::Statistics() const
{
	Tasks.Require({EDomain::Render});
	return LastStatistics;
}

void FRenderSession::Close()
{
	if (bClosed)
	{
		return;
	}
	Tasks.Require({EDomain::Main});
	Scene.Close();
	Resources.Close();
	bClosed = true;
}
} // namespace Hyperion
