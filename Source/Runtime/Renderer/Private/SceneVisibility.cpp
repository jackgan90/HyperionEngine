#include "Hyperion/Renderer/RenderResources.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace Hyperion
{
FRenderSceneSnapshot PrepareSceneSnapshot(FRenderSceneSnapshot InSnapshot)
{
	struct FOrderedItem
	{
		FRenderItem Item;
		bool bBlend{};
		float Depth{};
	};

	std::vector<FOrderedItem> Ordered;
	std::map<const FRenderResource*, std::shared_ptr<const FRenderResourceDesc>> Descriptions;
	for (auto& Item : InSnapshot.Items)
	{
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
			continue;
		}
		if (Item.State.Section >= Desc->Sections.size())
		{
			// Pending descriptions can reveal an invalid section after state admission.
			// The binding reports failure independently; other scene items remain renderable.
			continue;
		}
		const auto& Section = Desc->Sections[Item.State.Section];
		const auto& Geometry = Desc->Geometries[Section.Geometry];
		const auto& Material = Desc->Materials[Section.Material];
		const auto Clip =
		    Material.bClipSpace ? Item.State.World : Multiply(InSnapshot.View.ViewProjection, Item.State.World);
		const auto CullingClip =
		    Material.bClipSpace
		        ? Item.State.World
		        : Multiply(InSnapshot.View.CullingViewProjection.value_or(InSnapshot.View.ViewProjection),
		                   Item.State.World);
		if (InSnapshot.View.CullingMode != ESceneCullingMode::None &&
		    !FFrustum(CullingClip).Intersects(Geometry.Bounds))
		{
			continue;
		}
		const auto Center = ScaleVector(Add(Geometry.Bounds.Minimum, Geometry.Bounds.Maximum), .5f);
		const auto Projected = Transform(Clip, {Center.X, Center.Y, Center.Z, 1});
		float Depth = Projected.W > 0 ? Projected.Z / Projected.W : std::numeric_limits<float>::lowest();
		if (!std::isfinite(Depth))
		{
			Depth = std::numeric_limits<float>::lowest();
		}
		Ordered.push_back({std::move(Item), Material.Pipeline.bAlphaBlend, Depth});
	}
	std::stable_sort(Ordered.begin(), Ordered.end(),
	                 [](const FOrderedItem& InA, const FOrderedItem& InB)
	                 {
		                 return InA.bBlend != InB.bBlend ? !InA.bBlend : InA.bBlend && InA.Depth > InB.Depth;
	                 });
	InSnapshot.Items.clear();
	for (auto& Item : Ordered)
	{
		InSnapshot.Items.push_back(std::move(Item.Item));
	}
	InSnapshot.Statistics.VisibleItems = InSnapshot.Items.size();
	return InSnapshot;
}
} // namespace Hyperion
