#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderResources.h"
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
	Ordered.reserve(InSnapshot.Items.size());
	std::map<const FRenderResource*, std::shared_ptr<const FRenderResourceDesc>> Descriptions;
	for (std::size_t Index = 0; Index < InSnapshot.Items.size(); ++Index)
	{
		auto& Item = InSnapshot.Items[Index];
		if (!Item.State.bVisible || !Item.State.Resource)
		{
			continue;
		}
		auto [It, bInserted] = Descriptions.try_emplace(Item.State.Resource.get());
		if (bInserted)
		{
			It->second = Item.State.Resource->GetDescription();
		}
		const auto& Desc = It->second;
		if (!Desc)
		{
			Item.PreparationError = "Geometry resources are not ready";
			Ordered.push_back({Index, 0, 0});
			continue;
		}
		if (Item.State.Section >= Desc->Sections.size())
		{
			Item.PreparationError = "Invalid primitive section";
			Ordered.push_back({Index, 0, 0});
			continue;
		}
		const auto& Section = Desc->Sections[Item.State.Section];
		const auto& Geometry = Desc->Geometries[Section.Geometry];
		if (!Item.State.Surface)
		{
			Item.State.Surface = Item.State.Resource->GetMaterial(Item.State.Section);
		}
		const auto Surface = Item.State.Surface ? Item.State.Surface->GetSnapshot() : nullptr;
		if (InSnapshot.View.bSkipMissingPass && (!Surface || !Surface->Definition->HasPass(InSnapshot.View.Usage)))
		{
			continue;
		}
		const bool bConservative = InSnapshot.View.CullingMode != ESceneCullingMode::None &&
		                           !Item.State.bConservativeBounds && Surface &&
		                           std::any_of(Surface->Definition->GetDescription().Passes.begin(),
		                                       Surface->Definition->GetDescription().Passes.end(),
		                                       [](const auto& InPass)
		                                       {
			                                       return InPass.bRequiresConservativeBounds;
		                                       });
		if (!IsItemVisible(Item, InSnapshot.View, Geometry.Bounds, bConservative))
		{
			continue;
		}
		const auto Queue = Surface && Surface->Definition->HasPass(InSnapshot.View.Usage)
		                       ? Surface->Definition->GetPass(InSnapshot.View.Usage).Queue
		                       : EMaterialQueue::Opaque;
		const unsigned Bucket = Queue == EMaterialQueue::Overlay ? 2 : Queue == EMaterialQueue::Transparent ? 1 : 0;
		Ordered.push_back({Index, Bucket, SortDepth(Item, InSnapshot.View, Geometry.Bounds, Queue)});
	}
	std::stable_sort(Ordered.begin(), Ordered.end(),
	                 [](const FOrderedItem& InA, const FOrderedItem& InB)
	                 {
		                 return InA.Queue != InB.Queue ? InA.Queue < InB.Queue
		                                               : InA.Queue == 1 && InA.Depth > InB.Depth;
	                 });
	// Sort indices so parameter containers move only once, preserving stable queue/depth ordering.
	std::vector<FRenderItem> Items;
	Items.reserve(Ordered.size());
	for (const auto& Item : Ordered)
	{
		Items.push_back(std::move(InSnapshot.Items[Item.Index]));
	}
	InSnapshot.Items = std::move(Items);
	InSnapshot.Statistics.VisibleItems = InSnapshot.Items.size();
	return InSnapshot;
}
} // namespace Hyperion
