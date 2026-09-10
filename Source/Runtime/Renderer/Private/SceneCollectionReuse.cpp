#include "SceneCollectionReuse.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>

namespace Hyperion
{
bool FSceneCollectionReuse::Earlier(const FEntry& InA, const FEntry& InB)
{
	return InA.Slot != InB.Slot ? InA.Slot < InB.Slot : InA.Ordinal < InB.Ordinal;
}

void FSceneCollectionReuse::IndexItems(FRenderItemList& InItems)
{
	for (std::size_t Index = 0; Index < InItems.Size(); ++Index)
	{
		const auto& Item = InItems[Index];
		Sparse.push_back({Item.Primitive.Slot, Item.Ordinal, {&InItems, Index}});
	}
}

FSceneCollectionReuse::FSceneCollectionReuse(FRenderSceneSnapshot* InPrevious) : Previous(InPrevious)
{
	HYP_PERF_SCOPE_C(Detail, IndexPreviousSceneItems);
	if (!Previous)
	{
		return;
	}
	Sparse.reserve(Previous->Items.Size() + Previous->RetainedItems.Size());
	IndexItems(Previous->Items);
	IndexItems(Previous->RetainedItems);
	if (Sparse.empty())
	{
		return;
	}
	FirstSlot = Sparse.front().Slot;
	std::uint32_t LastSlot = FirstSlot;
	std::uint64_t LastOrdinal{};
	for (const auto& Entry : Sparse)
	{
		FirstSlot = std::min(FirstSlot, Entry.Slot);
		LastSlot = std::max(LastSlot, Entry.Slot);
		LastOrdinal = std::max(LastOrdinal, Entry.Ordinal);
	}
	const auto Range = std::uint64_t(LastSlot) - FirstSlot + 1;
	const auto DenseLimit = Sparse.size() < 8192 ? Sparse.size() * 8 + 64 : 65536;
	if (Range > DenseLimit || LastOrdinal >= 64)
	{
		std::sort(Sparse.begin(), Sparse.end(), Earlier);
		return; // Sparse generations or unusually large custom collections do not allocate a slot-sized table.
	}
	Slots.resize(static_cast<std::size_t>(Range));
	for (const auto& Entry : Sparse)
	{
		auto& Slot = Slots[Entry.Slot - FirstSlot];
		if (Entry.Ordinal == 0)
		{
			Slot.First = Entry.Location;
		}
		else
		{
			Slot.Additional.resize(std::max(Slot.Additional.size(), static_cast<std::size_t>(Entry.Ordinal)));
			Slot.Additional[Entry.Ordinal - 1] = Entry.Location;
		}
	}
	Sparse.clear();
}

FSceneCollectionReuse::FLocation FSceneCollectionReuse::Find(std::uint32_t InSlot, std::size_t InOrdinal) const
{
	if (!Slots.empty())
	{
		if (InSlot < FirstSlot || std::uint64_t(InSlot) - FirstSlot >= Slots.size())
		{
			return {};
		}
		const auto& Slot = Slots[InSlot - FirstSlot];
		return InOrdinal == 0                        ? Slot.First
		       : InOrdinal <= Slot.Additional.size() ? Slot.Additional[InOrdinal - 1]
		                                             : FLocation{};
	}
	const auto It = std::lower_bound(Sparse.begin(), Sparse.end(), FEntry{InSlot, InOrdinal}, Earlier);
	return It != Sparse.end() && It->Slot == InSlot && It->Ordinal == InOrdinal ? It->Location : FLocation{};
}

void FSceneCollectionReuse::Append(FRenderSceneSnapshot& OutSnapshot, std::span<const FRenderItem> InItems,
                                   FRenderPrimitiveHandle InHandle)
{
	HYP_PERF_SCOPE_C(Detail, AppendRetainedSceneItems);
	for (std::size_t Ordinal = 0; Ordinal < InItems.size(); ++Ordinal)
	{
		AppendItem(OutSnapshot, InItems[Ordinal], InHandle, Ordinal);
	}
}

void FSceneCollectionReuse::AppendItem(FRenderSceneSnapshot& OutSnapshot, const FRenderItem& InItem,
                                       FRenderPrimitiveHandle InHandle, std::size_t InOrdinal)
{
	const auto Source = Find(InHandle.Slot, InOrdinal);
	if (Source.Items)
	{
		const auto& Item = (*Source.Items)[Source.Index];
		if (Item.Primitive == InHandle && Item.Preparation == InItem.Preparation)
		{
			OutSnapshot.Items.MoveFrom(*Source.Items, Source.Index);
			Reused.resize(OutSnapshot.Items.Size());
			Reused.back() = true;
			++OutSnapshot.Statistics.ItemStorageReuses;
			OutSnapshot.Statistics.RetainedItemRestores += Source.Items == &Previous->RetainedItems;
			return;
		}
		Source.Items->Discard(Source.Index);
	}
	OutSnapshot.Items.PushBack(InItem);
}

bool FSceneCollectionReuse::IsReused(std::size_t InIndex) const
{
	return InIndex < Reused.size() && Reused[InIndex];
}

void FSceneCollectionReuse::RetainUnselected(FRenderSceneSnapshot& OutSnapshot)
{
	if (Previous && OutSnapshot.bRetainCulledItems)
	{
		// Most recently visible items get first admission. Neither cache can exceed the per-view retention limit.
		OutSnapshot.RetainedItems.MoveRemainingFrom(Previous->Items, FRenderSceneSnapshot::RetainedItemLimit);
		OutSnapshot.RetainedItems.MoveRemainingFrom(Previous->RetainedItems, FRenderSceneSnapshot::RetainedItemLimit);
	}
}
} // namespace Hyperion
