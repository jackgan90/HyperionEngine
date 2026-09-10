#pragma once
#include "Hyperion/Renderer/RenderPrimitive.h"
#include <span>

namespace Hyperion
{
class FSceneCollectionReuse
{
public:
	explicit FSceneCollectionReuse(FRenderSceneSnapshot* InPrevious);
	void Append(FRenderSceneSnapshot& OutSnapshot, std::span<const FRenderItem> InItems,
	            FRenderPrimitiveHandle InHandle);
	void AppendItem(FRenderSceneSnapshot& OutSnapshot, const FRenderItem& InItem, FRenderPrimitiveHandle InHandle,
	                std::size_t InOrdinal);
	bool IsReused(std::size_t InIndex) const;
	void RetainUnselected(FRenderSceneSnapshot& OutSnapshot);

private:
	struct FLocation
	{
		FRenderItemList* Items{};
		std::size_t Index{};
	};

	struct FEntry
	{
		std::uint32_t Slot{};
		std::uint64_t Ordinal{};
		FLocation Location;
	};

	struct FSlot
	{
		FLocation First;
		std::vector<FLocation> Additional;
	};

	FRenderSceneSnapshot* Previous{};
	std::vector<FEntry> Sparse;
	std::vector<FSlot> Slots;
	std::uint32_t FirstSlot{};
	std::vector<std::uint8_t> Reused;
	void IndexItems(FRenderItemList& InItems);
	FLocation Find(std::uint32_t InSlot, std::size_t InOrdinal) const;
	static bool Earlier(const FEntry& InA, const FEntry& InB);
};
} // namespace Hyperion
