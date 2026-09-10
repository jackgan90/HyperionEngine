#include "Hyperion/Core/Profiling.h"
#include "RenderSceneInternal.h"
#include "SceneCollectionReuse.h"
#include "SceneItemPreparation.h"
#include <algorithm>
#include <chrono>

namespace Hyperion
{
std::vector<FRenderItem> FRenderScene::FEntry::Collect(const FRenderView& InView, std::uint64_t InResourceRevision)
{
	std::vector<FRenderItem> Items;
	Primitive->Collect(InView, Items);
	if (Primitive->IsStaticCollection() && InResourceRevision)
	{
		bool bReady = Items.size() <= 64;
		for (auto& Item : Items)
		{
			bReady &= PrepareSceneItem(Item);
		}
		if (bReady)
		{
			Collection = Items;
			ResourceRevision = InResourceRevision;
		}
	}
	return Items;
}

void FRenderScene::RefreshSpatial(FSceneVisibilityStats& OutStats)
{
	HYP_PERF_SCOPE_C(Render, RefreshSpatial);
	const auto Start = std::chrono::steady_clock::now();
	DirtyGroups.insert(UnboundedGroups.begin(), UnboundedGroups.end());
	for (const auto Group : DirtyGroups)
	{
		FBounds Bounds;
		bool bFirst = true;
		for (const auto Slot : Groups.at(Group))
		{
			auto& Entry = Entries.at(Slot);
			Entry.Bounds = Entry.Primitive->GetWorldBounds();
			Bounds = bFirst ? Entry.Bounds : UnionBounds(Bounds, Entry.Bounds);
			bFirst = false;
		}
		Spatial->Set(Group, Bounds);
		if (IsUsable(Bounds))
		{
			UnboundedGroups.erase(Group);
		}
		else
		{
			UnboundedGroups.insert(Group);
		}
	}
	DirtyGroups.clear();
	OutStats.Primitives = Entries.size();
	OutStats.UpdateMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	Spatial->Commit(OutStats);
}

FSceneVisibilityStats FRenderScene::BeginViews()
{
	Tasks.Require({EDomain::Render});
	FSceneVisibilityStats Stats;
	RefreshSpatial(Stats);
	return Stats;
}

std::vector<FBounds> FRenderScene::QueryBounds(const ISceneVisibility& InVisibility) const
{
	Tasks.Require({EDomain::Render});
	FSceneVisibilityStats Stats;
	std::vector<FBounds> Result;
	for (const auto Group : Spatial->Query(&InVisibility, true, Stats))
	{
		for (const auto Slot : Groups.at(Group))
		{
			const auto& Entry = Entries.at(Slot);
			if (Entry.Primitive->GetState().bVisible && InVisibility.Intersects(Entry.Bounds))
			{
				Result.push_back(Entry.Bounds);
			}
		}
	}
	return Result;
}

FRenderSceneSnapshot FRenderScene::Collect(FRenderView InView, bool bInRefresh, std::uint64_t InResourceRevision,
                                           FRenderSceneSnapshot* InPrevious)
{
	HYP_PERF_SCOPE_C(Render, CollectScene);
	Tasks.Require({EDomain::Render});
	FRenderSceneSnapshot Snapshot;
	Snapshot.CollectionKey = {Identity, Revision, InResourceRevision};
	FRenderSceneSnapshot* Previous =
	    InPrevious && InPrevious->CollectionKey == Snapshot.CollectionKey ? InPrevious : nullptr;
	Snapshot.DrawFrame = Previous ? Previous->DrawFrame : std::make_shared<std::atomic_uint64_t>(0);
	Snapshot.bRetainCulledItems = InResourceRevision && GetCollectionRevision().has_value();
	FSceneCollectionReuse Reuse(Previous);
	auto& Stats = Snapshot.Statistics;
	if (bInRefresh)
	{
		RefreshSpatial(Stats);
	}
	Stats.Primitives = Entries.size();
	Stats.Groups = Groups.size();
	Stats.UnboundedGroups = UnboundedGroups.size();
	const FFrustumVisibility Frustum(InView.CullingViewProjection.value_or(InView.ViewProjection));
	const ISceneVisibility* Visibility = InView.CullingMode == ESceneCullingMode::None ? nullptr : &Frustum;
	const auto Candidates = Spatial->Query(Visibility, InView.CullingMode == ESceneCullingMode::Bvh, Stats);
	std::vector<std::uint32_t> Slots;
	for (const auto Group : Candidates)
	{
		const auto& Members = Groups.at(Group);
		Slots.insert(Slots.end(), Members.begin(), Members.end());
	}
	std::sort(Slots.begin(), Slots.end()); // Preserve registry order, including transparent ties.
	Stats.CandidatePrimitives = Slots.size();
	Snapshot.Items.Reserve(Slots.size());
	for (const auto Slot : Slots)
	{
		auto& Entry = Entries.at(Slot);
		if (!Entry.Primitive->GetState().bVisible || (Visibility && !Visibility->Intersects(Entry.Bounds)))
		{
			continue;
		}
		const auto Start = Snapshot.Items.Size();
		++Stats.CollectedPrimitives;
		if (Entry.Collection && Entry.ResourceRevision == InResourceRevision && InResourceRevision)
		{
			Reuse.Append(Snapshot, *Entry.Collection, Entry.Handle);
			Stats.ItemPreparationReuses += Entry.Collection->size();
		}
		else
		{
			Snapshot.Items.Append(Entry.Collect(InView, InResourceRevision));
			// Uncacheable emissions must not retain an older copy of the same local item.
			Snapshot.bRetainCulledItems &= Entry.Collection.has_value() && Entry.ResourceRevision == InResourceRevision;
		}
		for (auto Index = Start; Index < Snapshot.Items.Size(); ++Index)
		{
			if (Reuse.IsReused(Index))
			{
				continue; // Reused storage retains the same primitive, lifetime and receipt ownership.
			}
			auto& Item = Snapshot.Items[Index];
			Item.Primitive = Entry.Handle;
			Item.Group = Entry.Group;
			Item.Ordinal = Index - Start;
			Item.Lifetime = Entry.Lifetime;
			Item.EvaluationCache = Entry.EvaluationCache;
			Item.Report = [Result = Entry.Result, Frame = Snapshot.DrawFrame](FRenderDrawResult InResult)
			{
				std::lock_guard Lock(Result->Mutex);
				const auto PreviousFrame = Result->LastDrawFrame
				                               ? Result->LastDrawFrame->load(std::memory_order_acquire)
				                               : Result->LastDraw.Frame;
				if (InResult.Revision == Result->Status.Revision && InResult.Frame >= PreviousFrame)
				{
					Result->LastDraw = std::move(InResult);
					Result->LastDrawFrame = Frame;
				}
			};
		}
	}
	Reuse.RetainUnselected(Snapshot);
	Stats.EmittedItems = Snapshot.Items.Size();
	Snapshot.View = std::move(InView);
	return Snapshot;
}
} // namespace Hyperion
