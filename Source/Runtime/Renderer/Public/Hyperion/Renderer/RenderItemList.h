#pragma once
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>

namespace Hyperion
{
struct FRenderItem;

// Ordered ownership with stable item storage. Visibility/order changes move handles, not parameter containers.
// Copying explicitly clones items so a retained earlier snapshot remains independent.
class FRenderItemList
{
	using FStorage = std::vector<std::unique_ptr<FRenderItem>>;

public:
	// NOLINTNEXTLINE(readability-identifier-naming): engine template and boolean parameter prefixes.
	template<bool bInConst> class TIterator
	{
		using FBase = std::conditional_t<bInConst, FStorage::const_iterator, FStorage::iterator>;
		FBase Position;
		friend class FRenderItemList;

	public:
		// NOLINTBEGIN(readability-identifier-naming): standard iterator traits.
		using iterator_category = std::random_access_iterator_tag;
		using value_type = FRenderItem;
		using difference_type = std::ptrdiff_t;
		using reference = std::conditional_t<bInConst, const FRenderItem&, FRenderItem&>;
		using pointer = std::conditional_t<bInConst, const FRenderItem*, FRenderItem*>;
		// NOLINTEND(readability-identifier-naming)

		TIterator() = default;

		explicit TIterator(FBase InPosition) : Position(InPosition)
		{
		}

		reference operator*() const
		{
			return **Position;
		}

		pointer operator->() const
		{
			return Position->get();
		}

		reference operator[](difference_type InOffset) const
		{
			return **(Position + InOffset);
		}

		TIterator& operator++()
		{
			++Position;
			return *this;
		}

		TIterator operator++(int)
		{
			auto Previous = *this;
			++*this;
			return Previous;
		}

		TIterator& operator--()
		{
			--Position;
			return *this;
		}

		TIterator operator--(int)
		{
			auto Previous = *this;
			--*this;
			return Previous;
		}

		TIterator& operator+=(difference_type InOffset)
		{
			Position += InOffset;
			return *this;
		}

		TIterator& operator-=(difference_type InOffset)
		{
			Position -= InOffset;
			return *this;
		}

		TIterator operator+(difference_type InOffset) const
		{
			return TIterator(Position + InOffset);
		}

		TIterator operator-(difference_type InOffset) const
		{
			return TIterator(Position - InOffset);
		}

		difference_type operator-(const TIterator& InOther) const
		{
			return Position - InOther.Position;
		}

		bool operator==(const TIterator& InOther) const = default;
		auto operator<=>(const TIterator& InOther) const = default;

		friend TIterator operator+(difference_type InOffset, TIterator InIterator)
		{
			return InIterator + InOffset;
		}
	};

	FRenderItemList();
	FRenderItemList(std::initializer_list<FRenderItem> InItems);
	~FRenderItemList();
	FRenderItemList(const FRenderItemList& InOther);
	FRenderItemList& operator=(const FRenderItemList& InOther);
	FRenderItemList(FRenderItemList&& InOther) noexcept;
	FRenderItemList& operator=(FRenderItemList&& InOther) noexcept;

	std::size_t Size() const
	{
		return Storage.size();
	}

	bool IsEmpty() const
	{
		return Storage.empty();
	}

	void Reserve(std::size_t InSize)
	{
		Storage.reserve(InSize);
	}

	FRenderItem& operator[](std::size_t InIndex)
	{
		return *Storage[InIndex];
	}

	const FRenderItem& operator[](std::size_t InIndex) const
	{
		return *Storage[InIndex];
	}

	FRenderItem& At(std::size_t InIndex)
	{
		return *Storage.at(InIndex);
	}

	const FRenderItem& At(std::size_t InIndex) const
	{
		return *Storage.at(InIndex);
	}

	FRenderItem& Front()
	{
		return *Storage.front();
	}

	const FRenderItem& Front() const
	{
		return *Storage.front();
	}

	// NOLINTBEGIN(readability-identifier-naming): standard range access.
	TIterator<false> begin()
	{
		return TIterator<false>(Storage.begin());
	}

	TIterator<false> end()
	{
		return TIterator<false>(Storage.end());
	}

	TIterator<true> begin() const
	{
		return TIterator<true>(Storage.begin());
	}

	TIterator<true> end() const
	{
		return TIterator<true>(Storage.end());
	}

	// NOLINTEND(readability-identifier-naming)

	void PushBack(const FRenderItem& InItem);
	void PushBack(FRenderItem&& InItem);
	void Append(std::vector<FRenderItem> InItems);
	TIterator<false> Erase(TIterator<false> InBegin, TIterator<false> InEnd);
	void Clear();
	// Consumes a source slot. The source is only cleared/replaced after the current ordering transaction.
	void MoveFrom(FRenderItemList& InSource, std::size_t InIndex);

private:
	FStorage Storage;
};
} // namespace Hyperion
