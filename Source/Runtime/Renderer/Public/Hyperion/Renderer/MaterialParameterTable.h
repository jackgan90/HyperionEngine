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
	// One flat immutable overlay, shared by compatible scope refreshes. It never references an older table.
	struct FSharedValues
	{
		explicit FSharedValues(std::vector<std::optional<TValue>> InValues) : Values(std::move(InValues))
		{
			for (std::size_t Index = 0; Index < Values.size(); ++Index)
			{
				if (Values[Index])
				{
					Indices.push_back(Index);
				}
			}
		}

		const std::optional<TValue>& Get(std::size_t InIndex) const
		{
			return Values.at(InIndex);
		}

	private:
		friend class TMaterialParameterTable;
		std::vector<std::optional<TValue>> Values;
		std::vector<std::size_t> Indices;
	};

	// Identity-only proof of the local pages and overlay mask. It does not retain parameter/resource values.
	class FLocalIdentity
	{
	public:
		bool Matches(const TMaterialParameterTable& InTable) const
		{
			if (Count != InTable.Count || !bShared || !InTable.Shared || Indices != InTable.Shared->Indices)
			{
				return false;
			}
			for (std::size_t Index = 0; Index < Pages.size(); ++Index)
			{
				const auto& Page = InTable.GetPage(Index);
				if (Pages[Index].Owner.owner_before(Page) || Page.owner_before(Pages[Index].Owner) ||
				    Pages[Index].Revision != Page->Revision)
				{
					return false;
				}
			}
			return true;
		}

	private:
		friend class TMaterialParameterTable;

		struct FLocalPage
		{
			std::weak_ptr<const void> Owner;
			std::uint64_t Revision{};
		};

		std::size_t Count{};
		bool bShared{};
		std::vector<FLocalPage> Pages;
		std::vector<std::size_t> Indices;
	};

	FLocalIdentity GetLocalIdentity() const
	{
		FLocalIdentity Result;
		Result.Count = Count;
		Result.bShared = bool(Shared);
		if (Shared)
		{
			Result.Indices = Shared->Indices;
		}
		for (std::size_t Index = 0; Index < Count; Index += PageSize)
		{
			const auto& Page = GetPage(Index / PageSize);
			Result.Pages.push_back({Page, Page->Revision});
		}
		return Result;
	}

	void Reset(std::size_t InSize, const TValue& InValue = {})
	{
		Shared.reset();
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
			Page->Values.fill(InValue);
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
		if (Shared && Shared->Values[InIndex])
		{
			return *Shared->Values[InIndex];
		}
		return GetPage(InIndex / PageSize)->Values[InIndex % PageSize];
	}

	void Set(std::size_t InIndex, TValue InValue)
	{
		if (Get(InIndex) == InValue)
		{
			return;
		}
		if (Shared && Shared->Values[InIndex])
		{
			MaterializeShared();
		}
		SetBase(InIndex, std::move(InValue));
	}

	void SetShared(std::shared_ptr<const FSharedValues> InShared)
	{
		if (InShared && InShared->Values.size() != Count)
		{
			throw std::invalid_argument("Material shared parameter size");
		}
		if (Shared && (!InShared || Shared->Indices != InShared->Indices))
		{
			MaterializeShared();
		}
		Shared = std::move(InShared);
	}

	const void* GetSharedIdentity() const
	{
		return Shared.get();
	}

	bool SharesLocalValues(const TMaterialParameterTable& InOther) const
	{
		return Count == InOther.Count && InlinePages == InOther.InlinePages && OverflowPages == InOther.OverflowPages &&
		       Shared && InOther.Shared && Shared->Indices == InOther.Shared->Indices;
	}

	const void* GetPageIdentity(std::size_t InIndex) const
	{
		Get(InIndex);
		return Shared && Shared->Values[InIndex] ? static_cast<const void*>(Shared.get())
		                                         : GetPage(InIndex / PageSize).get();
	}

private:
	void SetBase(std::size_t InIndex, TValue InValue)
	{
		auto& Page = GetPage(InIndex / PageSize);
		if (Page.use_count() != 1)
		{
			Page = std::make_shared<FPage>(*Page);
		}
		Page->Values[InIndex % PageSize] = std::move(InValue);
		++Page->Revision;
	}

	void MaterializeShared()
	{
		// Construct before publishing so an allocation failure leaves the table intact.
		auto Materialized = *this;
		for (const auto Index : Shared->Indices)
		{
			Materialized.SetBase(Index, *Shared->Values[Index]);
		}
		Materialized.Shared.reset();
		*this = std::move(Materialized);
	}

	static constexpr std::size_t PageSize = 8;

	struct FPage
	{
		std::array<TValue, PageSize> Values;
		std::uint64_t Revision{};
	};

	std::array<std::shared_ptr<FPage>, 4> InlinePages;
	std::vector<std::shared_ptr<FPage>> OverflowPages;
	std::size_t Count{};
	std::shared_ptr<const FSharedValues> Shared;

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
