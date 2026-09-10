#include "Hyperion/Core/Profiling.h"
#include "LocalMaterialPreparation.h"
#include "RenderPassDeclaration.h"
#include "SessionMaterialsInternal.h"
#include <algorithm>
#include <bit>
#include <chrono>

namespace Hyperion
{
namespace
{
bool SameMatrix(const FMat4& InA, const FMat4& InB)
{
	return std::bit_cast<std::array<std::uint32_t, 16>>(InA.Values) ==
	       std::bit_cast<std::array<std::uint32_t, 16>>(InB.Values);
}

bool SameCollectionView(const FRenderView& InA, const FRenderView& InB, bool bInDepthSorted)
{
	return InA.Usage == InB.Usage && InA.bSkipMissingPass == InB.bSkipMissingPass &&
	       InA.CullingMode == InB.CullingMode &&
	       (InA.CullingMode == ESceneCullingMode::None ||
	        SameMatrix(InA.CullingViewProjection.value_or(InA.ViewProjection),
	                   InB.CullingViewProjection.value_or(InB.ViewProjection))) &&
	       (!bInDepthSorted || SameMatrix(InA.ViewProjection, InB.ViewProjection));
}

bool SamePassEnvironment(const FRenderView& InA, const FRenderView& InB)
{
	const auto Viewport = [](const FRenderView& InView)
	{
		const auto Value = InView.Viewport.value_or(FViewport{0, 0, float(InView.Width), float(InView.Height)});
		return std::array{Value.X, Value.Y, Value.Width, Value.Height, Value.MinDepth, Value.MaxDepth};
	};
	return InA.Identity == InB.Identity && InA.Usage == InB.Usage && InA.Width == InB.Width &&
	       InA.Height == InB.Height && InA.Viewport.has_value() == InB.Viewport.has_value() &&
	       Viewport(InA) == Viewport(InB) && InA.bInstanceBatching == InB.bInstanceBatching;
}

bool SamePreparedView(const FRenderView& InA, const FRenderView& InB)
{
	return SamePassEnvironment(InA, InB) && InA.Revision == InB.Revision &&
	       SameMatrix(InA.ViewProjection, InB.ViewProjection) &&
	       std::array{InA.Eye.X, InA.Eye.Y, InA.Eye.Z} == std::array{InB.Eye.X, InB.Eye.Y, InB.Eye.Z} &&
	       InA.Parameters == InB.Parameters && InA.PassParameters == InB.PassParameters;
}

bool SameEngineInputs(const FMaterialFrameContext& InA, const FMaterialFrameContext& InB, std::uint32_t InDependencies)
{
	constexpr auto Transient = MaterialScopeBit(EMaterialScope::Frame) | MaterialScopeBit(EMaterialScope::Pass) |
	                           MaterialScopeBit(EMaterialScope::Draw);
	if (InA.Session != InB.Session || (InDependencies & Transient))
	{
		return false;
	}
	for (const auto Scope : {EMaterialScope::Global, EMaterialScope::Scene})
	{
		const auto Index = static_cast<std::size_t>(Scope);
		if ((InDependencies & MaterialScopeBit(Scope)) &&
		    (InA.Inputs.Scopes[Index].Key != InB.Inputs.Scopes[Index].Key ||
		     InA.Inputs.Scopes[Index].Lifetime != InB.Inputs.Scopes[Index].Lifetime ||
		     InA.Inputs.Values[Index] != InB.Inputs.Values[Index]))
		{
			return false;
		}
	}
	return true;
}

bool HasDepthSortedItems(const FRenderSceneSnapshot& InSnapshot)
{
	return std::any_of(
	    InSnapshot.Items.begin(), InSnapshot.Items.end(),
	    [&](const auto& InItem)
	    {
		    return InItem.State.Surface &&
		           InItem.State.Surface->GetSnapshot()->Definition->HasPass(InSnapshot.View.Usage) &&
		           InItem.State.Surface->GetSnapshot()->Definition->GetPass(InSnapshot.View.Usage).Queue ==
		               EMaterialQueue::Transparent;
	    });
}

void ResetViewStatistics(FRenderSceneSnapshot& InSnapshot, bool bInReuseCollection, bool bInReusePreparation)
{
	auto& Stats = InSnapshot.Statistics;
	Stats.CollectionReuses = bInReuseCollection;
	Stats.PreparationReuses = bInReusePreparation;
	Stats.MaterialMilliseconds = 0;
	Stats.SharedMaterialUpdates = 0;
	Stats.SharedMaterialGroups = 0;
	Stats.RetainedMaterialItems = 0;
	if (bInReuseCollection)
	{
		Stats.MembershipReuses = 0;
		Stats.MembershipAdded = 0;
		Stats.MembershipRemoved = 0;
		Stats.ContainedItemTests = 0;
		Stats.RetainedItemRestores = 0;
		Stats.ItemPreparationReuses = InSnapshot.Items.Size();
		Stats.ItemStorageReuses = InSnapshot.Items.Size();
		Stats.QueryMilliseconds = 0;
		Stats.VisitedNodes = 0;
		Stats.GroupTests = 0;
	}
}
} // namespace

void FRenderSession::InvalidatePreparedViews()
{
	Batches.InvalidatePlans(true);
	// A failed native Close may be retried after the material state was already released.
	if (MaterialState)
	{
		MaterialState->PreparedViews.clear();
	}
}

std::shared_ptr<const FRenderSceneSnapshot> FRenderSession::PrepareView(
    const FRenderView& InView, const FRenderPassTargets& InTargets,
    std::shared_ptr<const FMaterialFrameContext> InFrame, std::uint64_t InFamily,
    std::optional<std::uint64_t> InSceneRevision, std::uint64_t InResourceRevision,
    std::vector<FRenderTargetSource>& OutReads)
{
	HYP_PERF_SCOPE_C(Render, PrepareRetainedView);
	FMaterialState::FPreparedView Uncached;
	auto& Cached = MaterialState->PreparedViews.size() >= 64 && !MaterialState->PreparedViews.contains(InView.Identity)
	                   ? Uncached
	                   : MaterialState->PreparedViews[InView.Identity];
	const bool bReuseCollection = InSceneRevision && Cached.Snapshot && Cached.bValid &&
	                              Cached.SceneRevision == *InSceneRevision &&
	                              Cached.ResourceRevision == InResourceRevision &&
	                              SameCollectionView(Cached.Snapshot->View, InView, Cached.bDepthSorted);
	const bool bReusePreparation =
	    bReuseCollection && !Batches.HasCustomStrategies() && Cached.Snapshot->Family == InFamily &&
	    SamePreparedView(Cached.Snapshot->View, InView) && Cached.Snapshot->Targets == InTargets &&
	    SameEngineInputs(*Cached.Snapshot->Frame, *InFrame, Cached.Dependencies);
	const bool bStableCollection = InSceneRevision && Cached.Snapshot && Cached.bValid &&
	                               Cached.SceneRevision == *InSceneRevision &&
	                               Cached.ResourceRevision == InResourceRevision;
	const bool bSameLocalEnvironment = bStableCollection && Cached.Snapshot->Family == InFamily &&
	                                   SamePassEnvironment(Cached.Snapshot->View, InView) &&
	                                   Cached.Snapshot->Targets == InTargets;
	if (!bReuseCollection)
	{
		if (bStableCollection && Cached.Snapshot.use_count() != 1)
		{
			Cached.Snapshot = std::make_shared<FRenderSceneSnapshot>(*Cached.Snapshot);
		}
		Cached.bValid = false;
		Cached.Snapshot = std::make_shared<FRenderSceneSnapshot>(
		    Scene.CollectPrepared(InView, InResourceRevision, bStableCollection ? Cached.Snapshot.get() : nullptr));
		Cached.bDepthSorted = HasDepthSortedItems(*Cached.Snapshot);
	}
	else if (Cached.Snapshot.use_count() != 1)
	{
		// An independently queued graph keeps the previous CPU snapshot intact.
		Cached.Snapshot = std::make_shared<FRenderSceneSnapshot>(*Cached.Snapshot);
	}
	auto& Snapshot = *Cached.Snapshot;
	Snapshot.View = InView;
	Snapshot.Frame = std::move(InFrame);
	Snapshot.Family = InFamily;
	Snapshot.Targets = InTargets;
	ResetViewStatistics(Snapshot, bReuseCollection, bReusePreparation);
	if (!bReusePreparation)
	{
		const auto Start = std::chrono::steady_clock::now();
		PrepareMaterials(Snapshot, bStableCollection);
		Snapshot.Statistics.MaterialMilliseconds =
		    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
		Cached.LocalPreparation =
		    InSceneRevision && !Batches.HasCustomStrategies()
		        ? PrepareLocalMaterials(Snapshot, bSameLocalEnvironment ? Cached.LocalPreparation : nullptr, Resources,
		                                bStableCollection)
		        : nullptr;
		Snapshot.LocalContentIdentity = Cached.LocalPreparation ? Cached.LocalPreparation->Lifetime : nullptr;
		Snapshot.Batches = Batches.Build(Snapshot, InView.bInstanceBatching);
		Cached.Dependencies = 0;
		Cached.bValid = true;
		for (const auto& Item : Snapshot.Items)
		{
			Cached.bValid &= Item.PreparationError.empty() && bool(Item.ResolvedParameters);
			Cached.Dependencies |= Item.ResolvedParameters ? Item.ResolvedParameters->DependenciesMask : 0;
		}
		Snapshot.ContentIdentity = Cached.bValid ? Resources.CreateScopeLifetime() : nullptr;
		Cached.GraphReads = CollectMaterialReads(Snapshot);
	}
	Cached.SceneRevision = InSceneRevision.value_or(0);
	Cached.ResourceRevision = InResourceRevision;
	Cached.AccessFrame = Snapshot.Frame->Frame;
	if (const auto It = MaterialState->Views.find(InView.Identity); It != MaterialState->Views.end())
	{
		It->second.AccessFrame = Snapshot.Frame->Frame;
	}
	HYP_PERF_PLOT(Render, SceneCollectionReuses, double(bReuseCollection));
	HYP_PERF_PLOT(Render, ViewPreparationReuses, double(bReusePreparation));
	OutReads = Cached.GraphReads;
	return Cached.Snapshot;
}
} // namespace Hyperion
