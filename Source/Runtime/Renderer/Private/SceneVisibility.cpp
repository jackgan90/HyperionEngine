#include "Hyperion/Renderer/RenderResources.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace Hyperion
{
namespace
{
bool Outside(const FBounds& InBounds, const FMat4& InClip)
{
	if (!InBounds.bValid)
	{
		return false;
	}
	std::array<bool, 6> Rejected{true, true, true, true, true, true};
	for (unsigned Corner = 0; Corner < 8; ++Corner)
	{
		const auto P = Transform(InClip, {Corner & 1 ? InBounds.Maximum.X : InBounds.Minimum.X,
		                                  Corner & 2 ? InBounds.Maximum.Y : InBounds.Minimum.Y,
		                                  Corner & 4 ? InBounds.Maximum.Z : InBounds.Minimum.Z, 1});
		if (!std::isfinite(P.X) || !std::isfinite(P.Y) || !std::isfinite(P.Z) || !std::isfinite(P.W))
		{
			return false;
		}
		const std::array Planes{P.X<-P.W, P.X> P.W, P.Y<-P.W, P.Y> P.W, P.Z<0, P.Z> P.W};
		for (std::size_t Index = 0; Index < Planes.size(); ++Index)
		{
			Rejected[Index] = Rejected[Index] && Planes[Index];
		}
	}
	return std::any_of(Rejected.begin(), Rejected.end(),
	                   [](bool bInValue)
	                   {
		                   return bInValue;
	                   });
}
} // namespace

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
		if (Outside(Geometry.Bounds, Clip))
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
	return InSnapshot;
}
} // namespace Hyperion
