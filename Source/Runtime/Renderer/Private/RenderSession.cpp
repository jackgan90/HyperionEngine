#include "Hyperion/Core/Profiling.h"
#include "SessionMaterialsInternal.h"
#include <chrono>
#include <stdexcept>

namespace Hyperion
{
namespace
{
template<typename TEntry> void RetireViewHistory(std::map<std::uint64_t, TEntry>& InEntries, std::uint64_t InFrame)
{
	std::erase_if(InEntries,
	              [&](const auto& InEntry)
	              {
		              return InFrame > InEntry.second.AccessFrame && InFrame - InEntry.second.AccessFrame > 120;
	              });
	while (InEntries.size() > 64)
	{
		const auto Oldest = std::min_element(InEntries.begin(), InEntries.end(),
		                                     [](const auto& InA, const auto& InB)
		                                     {
			                                     return InA.second.AccessFrame < InB.second.AccessFrame;
		                                     });
		InEntries.erase(Oldest);
	}
}
} // namespace

struct FRenderSession::FPreparedViewFamily
{
	std::vector<FRenderViewStatistics> Views;
	FSceneVisibilityStats Spatial;
	double Milliseconds{};
	bool bReady{};
	std::vector<FColorPass> Prepare(const FRenderResourcePreparation& InResources,
	                                std::span<const std::shared_ptr<const FRenderSceneSnapshot>> InSnapshots);
};

std::vector<FColorPass> FRenderSession::FPreparedViewFamily::Prepare(
    const FRenderResourcePreparation& InResources,
    std::span<const std::shared_ptr<const FRenderSceneSnapshot>> InSnapshots)
{
	bReady = false;
	const auto Start = std::chrono::steady_clock::now();
	std::vector<FColorPass> Passes;
	std::vector<FRenderViewStatistics> PreparedViews;
	for (const auto& Snapshot : InSnapshots)
	{
		const auto& Frame = *Snapshot;
		auto Stats = Frame.Statistics;
		auto Prepared = InResources.BuildPasses(Frame, &Stats.Batches);
		Stats.PacketReuses = Stats.Batches.PacketReuses;
		for (const auto& Pass : Prepared)
		{
			Stats.Draws += Pass.Commands.GetDraws().size();
		}
		PreparedViews.push_back({Frame.View.Identity, Frame.View.Usage, Stats});
		Passes.insert(Passes.end(), std::make_move_iterator(Prepared.begin()), std::make_move_iterator(Prepared.end()));
	}
	Views = std::move(PreparedViews);
	Milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	bReady = true;
	return Passes;
}

FRenderSession::FRenderSession(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler)
    : FRenderSession(InTasks, InDevice, InCompiler, ERHIDepthFormat::D32, GetStandardMaterialSemantics())
{
}

FRenderSession::FRenderSession(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                               ERHIDepthFormat InDepthFormat,
                               std::shared_ptr<const FMaterialSemanticRegistry> InSemantics)
    : Tasks(InTasks), Resources(InTasks, InDevice, InCompiler), Scene(
                                                                    InTasks,
                                                                    [this]
                                                                    {
	                                                                    return Resources.CreateScopeLifetime();
                                                                    },
                                                                    [this]
                                                                    {
	                                                                    InvalidatePreparedViews();
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
                                       bool bInSpatialPrepared, bool bInDeferPreparation)
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
	AdmitFamily(InViews, *InFrame, InFamily);
	const auto SpatialStats = bInSpatialPrepared ? FSceneVisibilityStats{} : Scene.BeginViews();
	std::vector<std::shared_ptr<const FRenderSceneSnapshot>> Snapshots;
	const auto SceneRevision = Scene.GetCollectionRevision();
	const auto ResourceRevision = Resources.GetPublicationRevision();
	std::size_t Count{};
	bool bRefreshed = false;
	LastStatistics = {};
	// Every Collect and provider evaluation finishes in this one Render task, before the first RHI wait.
	for (const auto& View : InViews)
	{
		auto Snapshot = PrepareView(View, InFrame, InFamily, SceneRevision, ResourceRevision);
		bRefreshed |= Snapshot->Statistics.PreparationReuses == 0;
		Count += Snapshot->Items.Size();
		Snapshots.push_back(std::move(Snapshot));
	}
	PendingFamily = std::make_shared<FPreparedViewFamily>();
	PendingFamily->Spatial = SpatialStats;
	auto Prepare =
	    [ResourcePreparation = Resources.GetPreparation(), Frames = std::move(Snapshots), Family = PendingFamily]
	{
		return Family->Prepare(ResourcePreparation, Frames);
	};
	if (bInDeferPreparation)
	{
		InGraph.AddDeferred(std::move(Prepare));
	}
	else
	{
		std::vector<FColorPass> Passes;
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Passes = Prepare();
		                          }));
		for (auto& Pass : Passes)
		{
			InGraph.Add(std::move(Pass));
		}
		CompleteViews();
	}
	RetireViewHistory(MaterialState->Views, InFrame->Frame);
	if (bRefreshed)
	{
		MaterialState->Providers.Collect();
	}
	RetireViewHistory(MaterialState->PreparedViews, InFrame->Frame);
	return Count;
}

double FRenderSession::CompleteViews()
{
	Tasks.Require({EDomain::Render});
	if (!PendingFamily || !PendingFamily->bReady)
	{
		throw std::logic_error("Deferred view preparation has not completed");
	}
	LastViews = PendingFamily->Views;
	LastStatistics = LastViews.back().Visibility;
	// Compatibility: Statistics reports the last view's visibility with family-wide draw/batch totals.
	LastStatistics.Draws = 0;
	LastStatistics.Batches = {};
	for (const auto& View : LastViews)
	{
		LastStatistics.Draws += View.Visibility.Draws;
		LastStatistics.Batches += View.Visibility.Batches;
	}
	LastStatistics.UpdateMilliseconds = PendingFamily->Spatial.UpdateMilliseconds;
	LastStatistics.IndexRebuilds = PendingFamily->Spatial.IndexRebuilds;
	LastStatistics.IndexRefits = PendingFamily->Spatial.IndexRefits;
	return PendingFamily->Milliseconds;
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
		                                  InvalidatePreparedViews();
	                                  });
	Tasks.Wait(Clear);
	Scene.Close();
	MaterialState.reset();
	Resources.Close();
	bClosed = true;
}
} // namespace Hyperion
