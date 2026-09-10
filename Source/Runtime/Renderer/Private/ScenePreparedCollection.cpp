#include "Hyperion/Core/Profiling.h"
#include "RenderSceneInternal.h"
#include "SceneCollectionReuse.h"
#include "SceneItemPreparation.h"
#include <algorithm>

namespace Hyperion
{
bool FRenderScene::SelectPrepared(const FRenderView& InView, std::uint64_t InResourceRevision,
                                  std::vector<FVisibleItem>& OutItems, FSceneVisibilityStats& OutStats)
{
	HYP_PERF_SCOPE_C(Render, SelectPreparedSceneItems);
	OutStats.Primitives = Entries.size();
	OutStats.Groups = Groups.size();
	OutStats.UnboundedGroups = UnboundedGroups.size();
	const auto& Matrix = InView.CullingViewProjection.value_or(InView.ViewProjection);
	const FFrustumVisibility Visibility(Matrix);
	const FFrustum Frustum(Matrix);
	const auto Candidates = Spatial->Query(InView.CullingMode == ESceneCullingMode::None ? nullptr : &Visibility,
	                                       InView.CullingMode == ESceneCullingMode::Bvh, OutStats);
	std::vector<std::uint32_t> Slots;
	for (const auto Group : Candidates)
	{
		const auto& Members = Groups.at(Group);
		Slots.insert(Slots.end(), Members.begin(), Members.end());
	}
	std::sort(Slots.begin(), Slots.end());
	OutStats.CandidatePrimitives = Slots.size();
	std::vector<FVisibleItem> Overlay;
	for (const auto Slot : Slots)
	{
		auto& Entry = Entries.at(Slot);
		if (!Entry.Primitive->GetState().bVisible ||
		    (InView.CullingMode != ESceneCullingMode::None && !Visibility.Intersects(Entry.Bounds)))
		{
			continue;
		}
		if (!Entry.Collection || Entry.ResourceRevision != InResourceRevision)
		{
			return false;
		}
		++OutStats.CollectedPrimitives;
		OutStats.EmittedItems += Entry.Collection->size();
		OutStats.ItemPreparationReuses += Entry.Collection->size();
		for (std::size_t Ordinal = 0; Ordinal < Entry.Collection->size(); ++Ordinal)
		{
			const auto& Item = (*Entry.Collection)[Ordinal];
			if (!Item.State.bVisible || !Item.State.Resource)
			{
				continue;
			}
			if (!Item.Preparation || !Item.Preparation->Program || !Item.State.Surface)
			{
				return false;
			}
			const auto& Definition = *Item.State.Surface->GetSnapshot()->Definition;
			const bool bHasPass = Definition.HasPass(InView.Usage);
			if (InView.bSkipMissingPass && !bHasPass)
			{
				continue;
			}
			const auto Queue = bHasPass ? Definition.GetPass(InView.Usage).Queue : EMaterialQueue::Opaque;
			if (Queue == EMaterialQueue::Transparent)
			{
				return false;
			}
			if (IsPreparedSceneItemVisible(Item, InView, Frustum, OutStats))
			{
				(Queue == EMaterialQueue::Overlay ? Overlay : OutItems).push_back({&Entry, Ordinal});
			}
		}
	}
	OutItems.insert(OutItems.end(), Overlay.begin(), Overlay.end());
	OutStats.VisibleItems = OutItems.size();
	return true;
}

void FRenderScene::MaterializePrepared(FRenderSceneSnapshot& OutSnapshot, const std::vector<FVisibleItem>& InItems,
                                       FRenderSceneSnapshot* InPrevious)
{
	HYP_PERF_SCOPE_C(Detail, MaterializeVisibleSceneItems);
	const auto PreviousCount = InPrevious ? InPrevious->Items.Size() : 0;
	FSceneCollectionReuse Reuse(InPrevious);
	OutSnapshot.Items.Reserve(InItems.size());
	for (const auto& Visible : InItems)
	{
		const auto& Entry = *Visible.Entry;
		const auto Index = OutSnapshot.Items.Size();
		Reuse.AppendItem(OutSnapshot, (*Entry.Collection)[Visible.Ordinal], Entry.Handle, Visible.Ordinal);
		if (!Reuse.IsReused(Index))
		{
			Entry.BindItem(OutSnapshot.Items[Index], Visible.Ordinal, OutSnapshot.DrawFrame);
		}
	}
	const auto Unchanged = OutSnapshot.Statistics.ItemStorageReuses - OutSnapshot.Statistics.RetainedItemRestores;
	OutSnapshot.Statistics.MembershipAdded = InItems.size() - Unchanged;
	OutSnapshot.Statistics.MembershipRemoved = PreviousCount - Unchanged;
	Reuse.RetainUnselected(OutSnapshot);
	OutSnapshot.Statistics.RetainedSceneItems = OutSnapshot.RetainedItems.Size();
}

FRenderSceneSnapshot FRenderScene::CollectPrepared(FRenderView InView, std::uint64_t InResourceRevision,
                                                   FRenderSceneSnapshot* InPrevious)
{
	Tasks.Require({EDomain::Render});
	const std::array<std::uint64_t, 3> Key{Identity, Revision, InResourceRevision};
	const bool bStable = InResourceRevision && GetCollectionRevision().has_value();
	FSceneVisibilityStats Stats;
	std::vector<FVisibleItem> Visible;
	if (!bStable || !SelectPrepared(InView, InResourceRevision, Visible, Stats))
	{
		return PrepareSceneSnapshot(Collect(std::move(InView), false, InResourceRevision, InPrevious));
	}
	auto* Previous = InPrevious && InPrevious->CollectionKey == Key ? InPrevious : nullptr;
	bool bSame = Previous && Previous->Items.Size() == Visible.size();
	for (std::size_t Index = 0; bSame && Index < Visible.size(); ++Index)
	{
		const auto& Item = Previous->Items[Index];
		const auto& Selection = Visible[Index];
		bSame = Item.Primitive == Selection.Entry->Handle && Item.Ordinal == Selection.Ordinal &&
		        Item.Preparation == (*Selection.Entry->Collection)[Selection.Ordinal].Preparation;
	}
	FRenderSceneSnapshot Snapshot;
	if (bSame)
	{
		Snapshot = std::move(*Previous);
		Stats.MembershipReuses = 1;
		Stats.ItemStorageReuses = Visible.size();
		Stats.RetainedSceneItems = Snapshot.RetainedItems.Size();
	}
	Snapshot.Statistics = Stats;
	Snapshot.CollectionKey = Key;
	Snapshot.bRetainCulledItems = true;
	if (!bSame)
	{
		Snapshot.DrawFrame = Previous ? Previous->DrawFrame : std::make_shared<std::atomic_uint64_t>(0);
		MaterializePrepared(Snapshot, Visible, Previous);
	}
	Snapshot.View = std::move(InView);
	return Snapshot;
}
} // namespace Hyperion
