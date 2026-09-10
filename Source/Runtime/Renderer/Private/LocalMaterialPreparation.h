#pragma once
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
class FRenderResourceService;

// Item-owned proof, published only by static collection material preparation. It owns no source values.
struct FLocalMaterialItem
{
	std::weak_ptr<const FSceneItemPreparation> Preparation;
	std::weak_ptr<const FResolvedMaterialParameters> Parameters;
};

// A view-owned certificate for ordered source/local values. Weak references never extend source lifetimes.
struct FLocalMaterialPreparation
{
	struct FItem
	{
		FRenderPrimitiveHandle Primitive;
		std::uint64_t Ordinal{};
		std::optional<std::uint64_t> LocalId;
		std::weak_ptr<const FSceneItemPreparation> Preparation;
		std::weak_ptr<const FResolvedMaterialParameters> Parameters;
	};

	std::vector<FItem> Items;
	std::shared_ptr<const void> Lifetime;
};

std::shared_ptr<const FLocalMaterialPreparation> PrepareLocalMaterials(
    FRenderSceneSnapshot& InSnapshot, const std::shared_ptr<const FLocalMaterialPreparation>& InPrevious,
    const FRenderResourceService& InResources, bool bInStableCollection);
} // namespace Hyperion
