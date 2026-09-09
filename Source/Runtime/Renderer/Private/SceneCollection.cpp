#include "Hyperion/Core/Profiling.h"
#include "RenderSceneInternal.h"
#include <algorithm>
#include <chrono>

namespace Hyperion
{
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

FRenderSceneSnapshot FRenderScene::Collect(FRenderView InView, bool bInRefresh)
{
	HYP_PERF_SCOPE_C(Render, CollectScene);
	Tasks.Require({EDomain::Render});
	FRenderSceneSnapshot Snapshot;
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
	Snapshot.Items.reserve(Slots.size());
	for (const auto Slot : Slots)
	{
		const auto& Entry = Entries.at(Slot);
		if (!Entry.Primitive->GetState().bVisible || (Visibility && !Visibility->Intersects(Entry.Bounds)))
		{
			continue;
		}
		const auto Start = Snapshot.Items.size();
		++Stats.CollectedPrimitives;
		Entry.Primitive->Collect(InView, Snapshot.Items);
		for (auto Index = Start; Index < Snapshot.Items.size(); ++Index)
		{
			auto& Item = Snapshot.Items[Index];
			Item.Primitive = Entry.Handle;
			Item.Group = Entry.Group;
			Item.Ordinal = Index - Start;
			Item.Lifetime = Entry.Lifetime;
			Item.EvaluationCache = Entry.EvaluationCache;
			Item.Report = [Result = Entry.Result](FRenderDrawResult InResult)
			{
				std::lock_guard Lock(Result->Mutex);
				if (InResult.Revision == Result->Status.Revision && InResult.Frame >= Result->LastDraw.Frame)
				{
					Result->LastDraw = std::move(InResult);
				}
			};
		}
	}
	Stats.EmittedItems = Snapshot.Items.size();
	Snapshot.View = std::move(InView);
	return Snapshot;
}
} // namespace Hyperion
