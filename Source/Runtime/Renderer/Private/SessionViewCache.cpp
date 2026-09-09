#include "Hyperion/Core/Profiling.h"
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

bool SamePreparedView(const FRenderView& InA, const FRenderView& InB)
{
	const auto Viewport = [](const FRenderView& InView)
	{
		const auto Value = InView.Viewport.value_or(FViewport{0, 0, float(InView.Width), float(InView.Height)});
		return std::array{Value.X, Value.Y, Value.Width, Value.Height, Value.MinDepth, Value.MaxDepth};
	};
	const auto Clear = [](const FRenderView& InView)
	{
		const auto Value = InView.ClearColor.value_or(FVec4{});
		return std::array{Value.X, Value.Y, Value.Z, Value.W};
	};
	return InA.Identity == InB.Identity && InA.Revision == InB.Revision && InA.Usage == InB.Usage &&
	       SameMatrix(InA.ViewProjection, InB.ViewProjection) &&
	       std::array{InA.Eye.X, InA.Eye.Y, InA.Eye.Z} == std::array{InB.Eye.X, InB.Eye.Y, InB.Eye.Z} &&
	       InA.Width == InB.Width && InA.Height == InB.Height && InA.Viewport.has_value() == InB.Viewport.has_value() &&
	       Viewport(InA) == Viewport(InB) && InA.Parameters == InB.Parameters &&
	       InA.PassParameters == InB.PassParameters && InA.bInstanceBatching == InB.bInstanceBatching &&
	       InA.DepthTarget == InB.DepthTarget && InA.SampledDepth == InB.SampledDepth &&
	       InA.TargetLifetime == InB.TargetLifetime && InA.Name == InB.Name &&
	       InA.ClearColor.has_value() == InB.ClearColor.has_value() && Clear(InA) == Clear(InB);
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
} // namespace

void FRenderSession::InvalidatePreparedViews()
{
	Batches.InvalidatePlans();
	// A failed native Close may be retried after the material state was already released.
	if (MaterialState)
	{
		MaterialState->PreparedViews.clear();
	}
}

std::shared_ptr<const FRenderSceneSnapshot> FRenderSession::PrepareView(
    const FRenderView& InView, std::shared_ptr<const FMaterialFrameContext> InFrame, std::uint64_t InFamily,
    std::optional<std::uint64_t> InSceneRevision, std::uint64_t InResourceRevision)
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
	const bool bReusePreparation = bReuseCollection && !Batches.HasCustomStrategies() &&
	                               Cached.Snapshot->Family == InFamily &&
	                               SamePreparedView(Cached.Snapshot->View, InView) &&
	                               SameEngineInputs(*Cached.Snapshot->Frame, *InFrame, Cached.Dependencies);
	if (!bReuseCollection)
	{
		const bool bReuseItems = InSceneRevision && Cached.Snapshot && Cached.bValid &&
		                         Cached.SceneRevision == *InSceneRevision &&
		                         Cached.ResourceRevision == InResourceRevision;
		if (bReuseItems && Cached.Snapshot.use_count() != 1)
		{
			Cached.Snapshot = std::make_shared<FRenderSceneSnapshot>(*Cached.Snapshot);
		}
		Cached.bValid = false;
		Cached.Snapshot = std::make_shared<FRenderSceneSnapshot>(PrepareSceneSnapshot(
		    Scene.Collect(InView, false, InResourceRevision, bReuseItems ? Cached.Snapshot.get() : nullptr)));
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
	Snapshot.DepthFormat = InView.DepthTarget ? ERHIDepthFormat::D32 : MaterialState->Depth;
	Snapshot.Statistics.CollectionReuses = bReuseCollection;
	Snapshot.Statistics.PreparationReuses = bReusePreparation;
	Snapshot.Statistics.MaterialMilliseconds = 0;
	Snapshot.Statistics.SharedMaterialUpdates = 0;
	if (bReuseCollection)
	{
		Snapshot.Statistics.ItemPreparationReuses = Snapshot.Items.Size();
		Snapshot.Statistics.ItemStorageReuses = Snapshot.Items.Size();
		Snapshot.Statistics.QueryMilliseconds = 0;
		Snapshot.Statistics.VisitedNodes = 0;
		Snapshot.Statistics.GroupTests = 0;
	}
	if (!bReusePreparation)
	{
		const auto Start = std::chrono::steady_clock::now();
		PrepareMaterials(Snapshot);
		Snapshot.Statistics.MaterialMilliseconds =
		    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
		Snapshot.Batches = Batches.Build(Snapshot, InView.bInstanceBatching);
		Cached.Dependencies = 0;
		Cached.bValid = true;
		for (const auto& Item : Snapshot.Items)
		{
			Cached.bValid &= Item.PreparationError.empty() && bool(Item.ResolvedParameters);
			Cached.Dependencies |= Item.ResolvedParameters ? Item.ResolvedParameters->DependenciesMask : 0;
		}
		Snapshot.ContentIdentity = Cached.bValid ? Resources.CreateScopeLifetime() : nullptr;
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
	return Cached.Snapshot;
}
} // namespace Hyperion
