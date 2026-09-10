#include "Hyperion/Core/Profiling.h"
#include "SessionMaterialsInternal.h"
#include <chrono>
#include <mutex>
#include <numeric>
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

struct FPreparedViewFamily
{
	std::vector<FRenderViewStatistics> Views;
	std::vector<double> Times;
	std::vector<bool> Prepared;
	FSceneVisibilityStats Spatial;
	double Milliseconds{};
	mutable std::mutex Publication;
	bool bReady{};
	std::vector<FGraphicsDrawBatch> Prepare(const FRenderResourcePreparation& InResources,
	                                        const FRenderSceneSnapshot& InSnapshot, std::size_t InIndex);
};

std::vector<FGraphicsDrawBatch> FPreparedViewFamily::Prepare(const FRenderResourcePreparation& InResources,
                                                             const FRenderSceneSnapshot& InSnapshot,
                                                             std::size_t InIndex)
{
	const auto Start = std::chrono::steady_clock::now();
	auto Stats = InSnapshot.Statistics;
	auto DrawBatches = InResources.BuildDraws(InSnapshot, &Stats.Batches);
	Stats.PacketReuses = Stats.Batches.PacketReuses;
	for (const auto& Batch : DrawBatches)
	{
		Stats.Draws += Batch.Commands.GetDraws().size();
	}
	std::lock_guard Lock(Publication);
	Views[InIndex] = {InSnapshot.View.Identity, InSnapshot.View.Usage, Stats};
	Times[InIndex] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	Prepared[InIndex] = true;
	Milliseconds = std::accumulate(Times.begin(), Times.end(), 0.0);
	bReady = std::all_of(Prepared.begin(), Prepared.end(),
	                     [](bool bInReady)
	                     {
		                     return bInReady;
	                     });
	return DrawBatches;
}

namespace
{
void PrepareImmediate(FTaskSystem& InTasks, std::vector<FGraphicsPass>& InPasses)
{
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              for (auto& Pass : InPasses)
		                              {
			                              Pass.Batches = Pass.Prepare();
			                              Pass.Prepare = {};
		                              }
	                              }));
}
} // namespace

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

FRenderPassTargets FRenderSession::FrameTargets(std::optional<FVec4> InClear) const
{
	if (bClosed)
	{
		throw std::logic_error("Closed render session");
	}
	return FRenderPassTargets::Frame(MaterialState->Depth, InClear);
}

std::size_t FRenderSession::Build(FRenderGraph& InGraph, FRenderView InView, FRenderPassTargets InTargets)
{
	return BuildViews(InGraph, std::span(&InView, 1), std::span(&InTargets, 1));
}

std::size_t FRenderSession::BuildViews(FRenderGraph& InGraph, std::span<const FRenderView> InViews,
                                       const FRenderPassTargets& InTargets,
                                       std::shared_ptr<const FMaterialFrameContext> InFrame, std::uint64_t InFamily,
                                       bool bInSpatialPrepared, bool bInDeferPreparation)
{
	const std::vector<FRenderPassTargets> Targets(InViews.size(), InTargets);
	return BuildViews(InGraph, InViews, Targets, std::move(InFrame), InFamily, bInSpatialPrepared, bInDeferPreparation);
}

std::size_t FRenderSession::BuildViews(FRenderGraph& InGraph, std::span<const FRenderView> InViews,
                                       std::span<const FRenderPassTargets> InTargets,
                                       std::shared_ptr<const FMaterialFrameContext> InFrame, std::uint64_t InFamily,
                                       bool bInSpatialPrepared, bool bInDeferPreparation)
{
	HYP_PERF_SCOPE_C(Render, BuildViews);
	Tasks.Require({EDomain::Render});
	if (bClosed || InViews.empty() || InFamily == 0 || InTargets.size() != InViews.size())
	{
		throw std::invalid_argument("Invalid or closed material view family and explicit targets");
	}
	if (!InFrame)
	{
		InFrame = MaterialState->Frame(Resources, 0, {}, Scene.GetLogicalSceneIdentity());
	}
	AdmitFamily(InViews, *InFrame, InFamily);
	const auto SpatialStats = bInSpatialPrepared ? FSceneVisibilityStats{} : Scene.BeginViews();
	const auto SceneRevision = Scene.GetCollectionRevision();
	const auto ResourceRevision = Resources.GetPublicationRevision();
	std::size_t Count{};
	bool bRefreshed = false;
	LastStatistics = {};
	PendingFamily = std::make_shared<FPreparedViewFamily>();
	PendingFamily->Spatial = SpatialStats;
	PendingFamily->Views.resize(InViews.size());
	PendingFamily->Times.resize(InViews.size());
	PendingFamily->Prepared.resize(InViews.size());
	std::vector<FGraphicsPass> Passes;
	// All collection, providers and graph declarations finish on Render before RHI preparation.
	for (std::size_t Index = 0; Index < InViews.size(); ++Index)
	{
		std::vector<FRenderTargetSource> Reads;
		auto Snapshot =
		    PrepareView(InViews[Index], InTargets[Index], InFrame, InFamily, SceneRevision, ResourceRevision, Reads);
		bRefreshed |= Snapshot->Statistics.PreparationReuses == 0;
		Count += Snapshot->Items.Size();
		auto Preparation = Resources.GetPreparation();
		auto Pass = Preparation.DeclarePass(InGraph, *Snapshot, Reads);
		Pass.Prepare = [Preparation, Snapshot = std::move(Snapshot), Family = PendingFamily, Index]
		{
			return Family->Prepare(Preparation, *Snapshot, Index);
		};
		Passes.push_back(std::move(Pass));
	}
	if (!bInDeferPreparation)
	{
		PrepareImmediate(Tasks, Passes);
		CompleteViews();
	}
	for (auto& Pass : Passes)
	{
		InGraph.Add(std::move(Pass));
	}
	RetireViewHistory(MaterialState->Views, InFrame->Frame);
	if (bRefreshed)
	{
		MaterialState->Providers.Collect();
	}
	RetireViewHistory(MaterialState->PreparedViews, InFrame->Frame);
	return Count;
}

FRenderViewFamilyStatistics FRenderViewPreparation::Statistics() const
{
	if (!State)
	{
		throw std::logic_error("View preparation is empty");
	}
	std::lock_guard Lock(State->Publication);
	if (!State->bReady)
	{
		throw std::logic_error("Deferred view preparation has not completed");
	}
	return {State->Views, State->Spatial, State->Milliseconds};
}

FRenderViewPreparation FRenderSession::GetViewPreparation() const
{
	Tasks.Require({EDomain::Render});
	FRenderViewPreparation Result;
	Result.State = PendingFamily;
	return Result;
}

double FRenderSession::CompleteViews()
{
	Tasks.Require({EDomain::Render});
	const auto Family = GetViewPreparation().Statistics();
	LastViews = Family.Views;
	LastStatistics = LastViews.back().Visibility;
	// Compatibility: Statistics reports the last view with family-wide draw/batch totals.
	LastStatistics.Draws = 0;
	LastStatistics.Batches = {};
	for (const auto& View : LastViews)
	{
		LastStatistics.Draws += View.Visibility.Draws;
		LastStatistics.Batches += View.Visibility.Batches;
	}
	LastStatistics.UpdateMilliseconds = Family.Spatial.UpdateMilliseconds;
	LastStatistics.IndexRebuilds = Family.Spatial.IndexRebuilds;
	LastStatistics.IndexRefits = Family.Spatial.IndexRefits;
	return Family.Milliseconds;
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
