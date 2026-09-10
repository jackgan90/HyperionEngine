#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
FRenderItemList::FRenderItemList() = default;

FRenderItemList::FRenderItemList(std::initializer_list<FRenderItem> InItems)
{
	Reserve(InItems.size());
	for (const auto& Item : InItems)
	{
		PushBack(Item);
	}
}

FRenderItemList::~FRenderItemList() = default;
FRenderItemList::FRenderItemList(FRenderItemList&& InOther) noexcept = default;
FRenderItemList& FRenderItemList::operator=(FRenderItemList&& InOther) noexcept = default;

FRenderItemList::FRenderItemList(const FRenderItemList& InOther)
{
	Storage.reserve(InOther.Storage.size());
	for (const auto& Item : InOther.Storage)
	{
		Storage.push_back(Item ? std::make_unique<FRenderItem>(*Item) : nullptr);
	}
}

FRenderItemList& FRenderItemList::operator=(const FRenderItemList& InOther)
{
	if (this != &InOther)
	{
		FRenderItemList Copy(InOther);
		Storage.swap(Copy.Storage);
	}
	return *this;
}

void FRenderItemList::PushBack(const FRenderItem& InItem)
{
	Storage.push_back(std::make_unique<FRenderItem>(InItem));
}

void FRenderItemList::PushBack(FRenderItem&& InItem)
{
	Storage.push_back(std::make_unique<FRenderItem>(std::move(InItem)));
}

void FRenderItemList::Append(std::vector<FRenderItem> InItems)
{
	Storage.reserve(Storage.size() + InItems.size());
	for (auto& Item : InItems)
	{
		PushBack(std::move(Item));
	}
}

FRenderItemList::TIterator<false> FRenderItemList::Erase(TIterator<false> InBegin, TIterator<false> InEnd)
{
	return TIterator<false>(Storage.erase(InBegin.Position, InEnd.Position));
}

void FRenderItemList::Clear()
{
	Storage.clear();
}

void FRenderItemList::MoveFrom(FRenderItemList& InSource, std::size_t InIndex)
{
	if (&InSource == this || !InSource.Storage.at(InIndex))
	{
		throw std::invalid_argument("Render item transfer requires an occupied slot in a different list");
	}
	Storage.push_back(std::move(InSource.Storage[InIndex]));
}

void FRenderItemList::MoveRemainingFrom(FRenderItemList& InSource, std::size_t InLimit)
{
	if (&InSource == this)
	{
		throw std::invalid_argument("Render item retention requires a different source list");
	}
	for (auto& Item : InSource.Storage)
	{
		if (Item && Storage.size() < InLimit)
		{
			Storage.push_back(std::move(Item));
		}
	}
	InSource.Clear();
}

void FRenderItemList::Discard(std::size_t InIndex)
{
	Storage.at(InIndex).reset();
}
} // namespace Hyperion
