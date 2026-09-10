#include "Hyperion/Core/Profiling.h"
#include "SceneItemPreparation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace Hyperion
{
namespace
{
bool IsItemVisible(const FRenderItem& InItem, const FRenderView& InView, const FBounds& InBounds, bool bInConservative)
{
	HYP_PERF_SCOPE_C(Detail, CullSceneItem);
	if (InView.CullingMode == ESceneCullingMode::None || (bInConservative && !InItem.State.bConservativeBounds))
	{
		return true;
	}
	const auto Clip = InItem.State.bClipSpace
	                      ? InItem.State.World
	                      : Multiply(InView.CullingViewProjection.value_or(InView.ViewProjection), InItem.State.World);
	return FFrustum(Clip).Intersects(IsUsable(InItem.State.LocalBounds) ? InItem.State.LocalBounds : InBounds);
}

float SortDepth(const FRenderItem& InItem, const FRenderView& InView, const FBounds& InBounds, EMaterialQueue InQueue)
{
	if (InQueue != EMaterialQueue::Transparent)
	{
		return 0;
	}
	const auto Clip =
	    InItem.State.bClipSpace ? InItem.State.World : Multiply(InView.ViewProjection, InItem.State.World);
	const auto Center = ScaleVector(Add(InBounds.Minimum, InBounds.Maximum), .5f);
	const auto Projected = Transform(Clip, {Center.X, Center.Y, Center.Z, 1});
	const float Depth = Projected.W > 0 ? Projected.Z / Projected.W : std::numeric_limits<float>::lowest();
	return std::isfinite(Depth) ? Depth : std::numeric_limits<float>::lowest();
}
} // namespace

bool PrepareSceneItem(FRenderItem& InItem)
{
	if (!InItem.State.Resource)
	{
		return false;
	}
	const auto Description = InItem.State.Resource->GetDescription();
	if (!Description || InItem.State.Section >= Description->Sections.size())
	{
		InItem.PreparationError = Description ? "Invalid primitive section" : "Geometry resources are not ready";
		return false;
	}
	if (!InItem.State.Surface)
	{
		InItem.State.Surface = InItem.State.Resource->GetMaterial(InItem.State.Section);
	}
	const auto Program = InItem.State.Surface ? InItem.State.Surface->GetCompiled() : nullptr;
	auto Result = std::make_shared<FSceneItemPreparation>();
	Result->Program = Program && InItem.State.Surface->GetStatus() == ERenderMaterialStatus::Ready ? Program : nullptr;
	Result->GeometryBounds = Description->Geometries[Description->Sections[InItem.State.Section].Geometry].Bounds;
	if (!InItem.State.bClipSpace)
	{
		Result->WorldBounds = TransformBounds(
		    IsUsable(InItem.State.LocalBounds) ? InItem.State.LocalBounds : Result->GeometryBounds, InItem.State.World);
	}
	if (InItem.State.Surface)
	{
		const auto& Passes = InItem.State.Surface->GetSnapshot()->Definition->GetDescription().Passes;
		Result->bRequiresConservativeBounds = std::any_of(Passes.begin(), Passes.end(),
		                                                  [](const auto& InPass)
		                                                  {
			                                                  return InPass.bRequiresConservativeBounds;
		                                                  });
	}
	if (!Result->Program)
	{
		InItem.Preparation = std::move(Result);
		return false;
	}
	try
	{
		InItem.Context.ObjectParameters = GetPrimitiveMaterialOverrides(InItem.State, *Program->Interface.Schema);
	}
	catch (const std::exception&)
	{
		Result->Program.reset();
		InItem.Preparation = std::move(Result);
		return false; // Preserve ordinary material preparation's per-item failure publication.
	}
	InItem.Preparation = std::move(Result);
	return true;
}

bool IsPreparedSceneItemVisible(const FRenderItem& InItem, const FRenderView& InView, const FFrustum& InFrustum,
                                FSceneVisibilityStats& OutStats)
{
	const auto& Preparation = *InItem.Preparation;
	if (InFrustum.Contains(Preparation.WorldBounds))
	{
		++OutStats.ContainedItemTests;
		return true;
	}
	return IsItemVisible(InItem, InView, Preparation.GeometryBounds, Preparation.bRequiresConservativeBounds);
}

FRenderSceneSnapshot PrepareSceneSnapshot(FRenderSceneSnapshot InSnapshot)
{
	HYP_PERF_SCOPE_C(Render, PrepareSceneSnapshot);

	struct FOrderedItem
	{
		std::size_t Index{};
		unsigned Queue{};
		float Depth{};
	};

	std::vector<FOrderedItem> Ordered;
	Ordered.reserve(InSnapshot.Items.Size());
	for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
	{
		auto& Item = InSnapshot.Items[Index];
		if (!Item.State.bVisible || !Item.State.Resource)
		{
			continue;
		}
		if (!Item.Preparation)
		{
			PrepareSceneItem(Item);
		}
		if (!Item.Preparation)
		{
			Ordered.push_back({Index, 0, 0});
			continue;
		}
		const auto& Preparation = *Item.Preparation;
		const auto Surface = Item.State.Surface ? Item.State.Surface->GetSnapshot() : nullptr;
		if (InSnapshot.View.bSkipMissingPass && (!Surface || !Surface->Definition->HasPass(InSnapshot.View.Usage)))
		{
			continue;
		}
		if (!IsItemVisible(Item, InSnapshot.View, Preparation.GeometryBounds, Preparation.bRequiresConservativeBounds))
		{
			continue;
		}
		const auto Queue = Surface && Surface->Definition->HasPass(InSnapshot.View.Usage)
		                       ? Surface->Definition->GetPass(InSnapshot.View.Usage).Queue
		                       : EMaterialQueue::Opaque;
		const unsigned Bucket = Queue == EMaterialQueue::Overlay ? 2 : Queue == EMaterialQueue::Transparent ? 1 : 0;
		Ordered.push_back({Index, Bucket, SortDepth(Item, InSnapshot.View, Preparation.GeometryBounds, Queue)});
	}
	HYP_PERF_SCOPE_C(Detail, PublishOrderedSceneItems);
	std::stable_sort(Ordered.begin(), Ordered.end(),
	                 [](const FOrderedItem& InA, const FOrderedItem& InB)
	                 {
		                 return InA.Queue != InB.Queue ? InA.Queue < InB.Queue
		                                               : InA.Queue == 1 && InA.Depth > InB.Depth;
	                 });
	// Sort indices so parameter containers move only once, preserving stable queue/depth ordering.
	FRenderItemList Items;
	Items.Reserve(Ordered.size());
	for (const auto& Item : Ordered)
	{
		Items.MoveFrom(InSnapshot.Items, Item.Index);
	}
	if (InSnapshot.bRetainCulledItems)
	{
		InSnapshot.RetainedItems.MoveRemainingFrom(InSnapshot.Items, FRenderSceneSnapshot::RetainedItemLimit);
	}
	InSnapshot.Items = std::move(Items);
	InSnapshot.Statistics.VisibleItems = InSnapshot.Items.Size();
	InSnapshot.Statistics.RetainedSceneItems = InSnapshot.RetainedItems.Size();
	return InSnapshot;
}
} // namespace Hyperion
