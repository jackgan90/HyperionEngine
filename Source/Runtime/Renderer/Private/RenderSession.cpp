#include "Hyperion/Core/Profiling.h"
#include "SessionMaterialsInternal.h"
#include <chrono>
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FPreparedViewFamily
{
	std::vector<FColorPass> Passes;
	std::vector<FRenderViewStatistics> Views;
};
} // namespace

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
      Batches(InTasks, InDevice.GetCapabilities()),
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

FRenderBatchSystem& FRenderSession::GetBatchSystem()
{
	Tasks.Require({EDomain::Main});
	return Batches;
}

std::size_t FRenderSession::Build(FRenderGraph& InGraph, FRenderView InView)
{
	return BuildViews(InGraph, std::span(&InView, 1));
}

std::size_t FRenderSession::BuildViews(FRenderGraph& InGraph, std::span<const FRenderView> InViews,
                                       std::shared_ptr<const FMaterialFrameContext> InFrame, std::uint64_t InFamily,
                                       bool bInSpatialPrepared)
{
	HYP_PERF_SCOPE_C(Render, BuildViews);
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
	const auto SpatialStats = bInSpatialPrepared ? FSceneVisibilityStats{} : Scene.BeginViews();
	std::vector<FRenderSceneSnapshot> Snapshots;
	std::size_t Count{};
	LastStatistics = {};
	// Every Collect and provider evaluation finishes in this one Render task, before the first RHI wait.
	for (const auto& View : InViews)
	{
		auto Snapshot = PrepareSceneSnapshot(Scene.Collect(View, false));
		Snapshot.Frame = InFrame;
		Snapshot.Family = InFamily;
		Snapshot.DepthFormat = View.DepthTarget ? ERHIDepthFormat::D32 : MaterialState->Depth;
		const auto MaterialStart = std::chrono::steady_clock::now();
		PrepareMaterials(Snapshot);
		Snapshot.Statistics.MaterialMilliseconds =
		    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - MaterialStart).count();
		Snapshot.Batches = Batches.Build(Snapshot, View.bInstanceBatching);
		Count += Snapshot.Items.size();
		Snapshots.push_back(std::move(Snapshot));
	}
	FPreparedViewFamily Family;
	const auto Preparation =
	    Tasks.Dispatch({EDomain::Rhi, 0},
	                   [ResourceService = &Resources, Frames = std::move(Snapshots), &Family]
	                   {
		                   for (const auto& Frame : Frames)
		                   {
			                   auto Prepared = ResourceService->BuildPasses(Frame);
			                   auto Stats = Frame.Statistics;
			                   Stats.Batches = ResourceService->Statistics().Batches;
			                   for (const auto& Pass : Prepared)
			                   {
				                   Stats.Draws += Pass.Commands.Draws.size();
			                   }
			                   Family.Views.push_back({Frame.View.Identity, Frame.View.Usage, Stats});
			                   Family.Passes.insert(Family.Passes.end(), std::make_move_iterator(Prepared.begin()),
			                                        std::make_move_iterator(Prepared.end()));
		                   }
	                   });
	Tasks.Wait(Preparation);
	LastViews = std::move(Family.Views);
	LastStatistics = LastViews.back().Visibility;
	// Compatibility: Statistics reports the last view's visibility with family-wide draw/batch totals.
	LastStatistics.Draws = 0;
	LastStatistics.Batches = {};
	for (const auto& View : LastViews)
	{
		LastStatistics.Draws += View.Visibility.Draws;
		LastStatistics.Batches += View.Visibility.Batches;
	}
	LastStatistics.UpdateMilliseconds = SpatialStats.UpdateMilliseconds;
	LastStatistics.IndexRebuilds = SpatialStats.IndexRebuilds;
	LastStatistics.IndexRefits = SpatialStats.IndexRefits;
	for (auto& Pass : Family.Passes)
	{
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

const std::vector<FRenderViewStatistics>& FRenderSession::ViewStatistics() const
{
	Tasks.Require({EDomain::Render});
	return LastViews;
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
	const auto Clear = Tasks.Dispatch({EDomain::Render},
	                                  [this]
	                                  {
		                                  Batches.Clear();
	                                  });
	Tasks.Wait(Clear);
	Scene.Close();
	MaterialState.reset();
	Resources.Close();
	bClosed = true;
}
} // namespace Hyperion
