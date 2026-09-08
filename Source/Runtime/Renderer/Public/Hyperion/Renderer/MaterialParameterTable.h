#pragma once
#include "Hyperion/Materials/MaterialParameters.h"
#include <array>
#include <stdexcept>

namespace Hyperion
{
// Bounded-depth copy-on-write storage. A refresh copies eight handles at most per affected page,
// never the trees behind those handles or a chain of prior evaluations.
template<typename TValue> class TMaterialParameterTable
{
public:
	void Reset(std::size_t InSize, const TValue& InValue = {})
	{
		Count = InSize;
		InlinePages = {};
		OverflowPages.clear();
		const auto PageCount = (Count + PageSize - 1) / PageSize;
		if (PageCount > InlinePages.size())
		{
			OverflowPages.resize(PageCount - InlinePages.size());
		}
		for (std::size_t Index = 0; Index < Count; Index += PageSize)
		{
			auto Page = std::make_shared<FPage>();
			Page->fill(InValue);
			GetPage(Index / PageSize) = std::move(Page);
		}
	}

	std::size_t GetSize() const
	{
		return Count;
	}

	const TValue& operator[](std::size_t InIndex) const
	{
		return Get(InIndex);
	}

	const TValue& Get(std::size_t InIndex) const
	{
		if (InIndex >= Count)
		{
			throw std::out_of_range("Material parameter index");
		}
		return (*GetPage(InIndex / PageSize))[InIndex % PageSize];
	}

	void Set(std::size_t InIndex, TValue InValue)
	{
		if (Get(InIndex) == InValue)
		{
			return;
		}
		auto& Page = GetPage(InIndex / PageSize);
		if (Page.use_count() != 1)
		{
			Page = std::make_shared<FPage>(*Page);
		}
		(*Page)[InIndex % PageSize] = std::move(InValue);
	}

	const void* GetPageIdentity(std::size_t InIndex) const
	{
		Get(InIndex);
		return GetPage(InIndex / PageSize).get();
	}

private:
	static constexpr std::size_t PageSize = 8;
	using FPage = std::array<TValue, PageSize>;
	std::array<std::shared_ptr<FPage>, 4> InlinePages;
	std::vector<std::shared_ptr<FPage>> OverflowPages;
	std::size_t Count{};

	std::shared_ptr<FPage>& GetPage(std::size_t InIndex)
	{
		return InIndex < InlinePages.size() ? InlinePages[InIndex] : OverflowPages.at(InIndex - InlinePages.size());
	}

	const std::shared_ptr<FPage>& GetPage(std::size_t InIndex) const
	{
		return InIndex < InlinePages.size() ? InlinePages[InIndex] : OverflowPages.at(InIndex - InlinePages.size());
	}
};

using FMaterialValueTable = TMaterialParameterTable<std::shared_ptr<const FMaterialValue>>;
using FMaterialDependencyTable = TMaterialParameterTable<std::uint32_t>;
} // namespace Hyperion
