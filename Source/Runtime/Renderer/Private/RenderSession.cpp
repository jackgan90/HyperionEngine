#include "SessionMaterialsInternal.h"
#include <stdexcept>

namespace Hyperion
{
FRenderSession::FRenderSession(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler)
    : FRenderSession(InTasks, InDevice, InCompiler, ERHIDepthFormat::D32, GetStandardMaterialSemantics())
{
}

FRenderSession::FRenderSession(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                               ERHIDepthFormat InDepthFormat,
                               std::shared_ptr<const FMaterialSemanticRegistry> InSemantics)
    : Tasks(InTasks), Resources(InTasks, InDevice, InCompiler), Scene(InTasks,
                                                                      [this]
                                                                      {
	                                                                      return Resources.CreateScopeLifetime();
                                                                      }),
      MaterialState(std::make_unique<FMaterialState>(InDepthFormat, std::move(InSemantics)))
{
	for (auto& Scope : MaterialState->Inputs.Scopes)
	{
		Scope = {{MaterialState->Identity, 1}, Resources.CreateScopeLifetime()};
	}
	SetSceneParameters(
	    {{"Engine.Scene.MainDirectionalLightDirection", FMaterialValue::Float(Normalize(FVec3{-.45f, .8f, .65f}))},
	     {"Engine.Scene.MainDirectionalLightColor", FMaterialValue::Float(FVec3{3.f, 2.85f, 2.7f})},
	     {"Engine.Scene.AmbientColor", FMaterialValue::Float(FVec3{.22f, .25f, .3f})}});
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
	return BuildViews(InGraph, std::span(&InView, 1));
}

std::size_t FRenderSession::BuildViews(FRenderGraph& InGraph, std::span<const FRenderView> InViews,
                                       std::shared_ptr<const FMaterialFrameContext> InFrame, std::uint64_t InFamily)
{
	Tasks.Require({EDomain::Render});
	if (bClosed || InViews.empty() || InFamily == 0)
	{
		throw std::invalid_argument("Invalid or closed material view family");
	}
	if (!InFrame)
	{
		InFrame = MaterialState->Frame(Resources, 0, {}, Scene.GetLogicalSceneIdentity());
	}
	const auto ViewIds = AdmitFamily(InViews, *InFrame, InFamily);
	std::vector<FRenderSceneSnapshot> Snapshots;
	std::size_t Count{};
	LastStatistics = {};
	// Every Collect and provider evaluation finishes in this one Render task, before the first RHI wait.
	for (const auto& View : InViews)
	{
		auto Snapshot = PrepareSceneSnapshot(Scene.Collect(View));
		Snapshot.Frame = InFrame;
		Snapshot.Family = InFamily;
		Snapshot.DepthFormat = MaterialState->Depth;
		PrepareMaterials(Snapshot);
		Count += Snapshot.Items.size();
		LastStatistics = Snapshot.Statistics;
		Snapshots.push_back(std::move(Snapshot));
	}
	auto Result = DispatchAsync<std::vector<FColorPass>>(
	    Tasks, {EDomain::Rhi, 0},
	    [ResourceService = &Resources, Frames = std::move(Snapshots)]
	    {
		    std::vector<FColorPass> Passes;
		    for (const auto& Frame : Frames)
		    {
			    auto Prepared = ResourceService->BuildPasses(Frame);
			    Passes.insert(Passes.end(), std::make_move_iterator(Prepared.begin()),
			                  std::make_move_iterator(Prepared.end()));
		    }
		    return Passes;
	    });
	Tasks.Wait(Result.Task());
	for (auto Pass : *Result.GetReady())
	{
		LastStatistics.Draws += Pass.Commands.Draws.size();
		InGraph.Add(std::move(Pass));
	}
	std::erase_if(MaterialState->Views,
	              [&](const auto& InEntry)
	              {
		              return !ViewIds.contains(InEntry.first);
	              });
	MaterialState->Providers.Collect();
	return Count;
}

std::set<std::uint64_t> FRenderSession::AdmitFamily(std::span<const FRenderView> InViews,
                                                    const FMaterialFrameContext& InFrame, std::uint64_t InFamily)
{
	if (InFrame.Session != MaterialState->Identity || InFrame.Frame < MaterialState->LastFrame)
	{
		throw std::invalid_argument("Foreign or stale material frame");
	}
	std::set<std::uint64_t> ViewIds;
	for (const auto& View : InViews)
	{
		if (View.Identity == 0 || View.Revision == 0 || View.Usage.empty() || !ViewIds.insert(View.Identity).second)
		{
			throw std::invalid_argument("Invalid or duplicate view identity in material family");
		}
	}
	if (MaterialState->LastFrame != InFrame.Frame)
	{
		MaterialState->LastFrame = InFrame.Frame;
		MaterialState->Families.clear();
	}
	if (!MaterialState->Families.insert(InFamily).second)
	{
		throw std::invalid_argument("Duplicate material family admission");
	}
	return ViewIds;
}

FSceneVisibilityStats FRenderSession::Statistics() const
{
	Tasks.Require({EDomain::Render});
	return LastStatistics;
}

FMaterialProviderStats FRenderSession::ProviderStatistics() const
{
	Tasks.Require({EDomain::Render});
	return MaterialState->Providers.Statistics();
}

void FRenderSession::Close()
{
	if (bClosed)
	{
		return;
	}
	Tasks.Require({EDomain::Main});
	Scene.Close();
	MaterialState.reset();
	Resources.Close();
	bClosed = true;
}
} // namespace Hyperion
