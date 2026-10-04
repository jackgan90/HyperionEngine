#include "RenderCacheKeys.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace
{
using namespace Hyperion;

void Check(bool bInCondition)
{
	if (!bInCondition)
	{
		throw std::runtime_error("Option/cache contract check failed");
	}
}

auto Legacy(const FBatchItemKey& InKey)
{
	return std::tuple{InKey.View,       InKey.Usage,     InKey.Scene,          InKey.Slot,
	                  InKey.Generation, InKey.LocalItem, InKey.DepthConvention};
}

void CheckBatchKeys()
{
	std::vector<FBatchItemKey> Keys;
	for (std::uint64_t Index = 0; Index < 128; ++Index)
	{
		Keys.push_back({.View = Index % 3,
		                .Usage = Index % 2 ? "Shadow" : "Main",
		                .Scene = Index % 5,
		                .Slot = std::uint32_t(Index % 7),
		                .Generation = Index % 11,
		                .LocalItem = Index % 13,
		                .DepthConvention = Index % 2 ? EDepthConvention::Standard : EDepthConvention::Reversed});
	}
	Keys.push_back(FBatchItemKey::ViewBegin(1, "Main"));
	std::map<FBatchItemKey, unsigned> Entries;
	for (const auto& Key : Keys)
	{
		const auto Old = Legacy(Key);
		std::size_t Hash = std::hash<std::string>{}(std::get<1>(Old));
		for (const auto Word : {std::get<0>(Old), std::get<2>(Old), std::uint64_t(std::get<3>(Old)), std::get<4>(Old),
		                        std::get<5>(Old), std::uint64_t(std::get<6>(Old))})
		{
			Hash = Hash * 16777619U ^ std::hash<std::uint64_t>{}(Word);
		}
		Check(FBatchItemKeyHash{}(Key) == Hash);
		for (const auto& Other : Keys)
		{
			Check((Key == Other) == (Old == Legacy(Other)));
			Check((Key < Other) == (Old < Legacy(Other)));
		}
		Entries.emplace(Key, 0);
	}
	for (std::uint64_t View = 0; View < 4; ++View)
	{
		for (const std::string Usage : {"Main", "Shadow", "Unused"})
		{
			std::size_t Count{};
			for (auto It = Entries.lower_bound(FBatchItemKey::ViewBegin(View, Usage));
			     It != Entries.end() && It->first.MatchesView(View, Usage); ++It)
			{
				++Count;
			}
			std::size_t Expected{};
			for (const auto& [Key, Value] : Entries)
			{
				Expected += std::get<0>(Legacy(Key)) == View && std::get<1>(Legacy(Key)) == Usage;
			}
			Check(Count == Expected);
		}
	}
}

void CheckInstanceKeys()
{
	for (std::uint64_t Index = 0; Index < 128; ++Index)
	{
		const FInstanceRecordKey Key{Index, Index + 1, std::uint32_t(Index + 2), Index + 3, Index + 4};
		const auto Old = std::tuple{Key.Layout, Key.Scene, Key.Slot, Key.Generation, Key.LocalItem};
		std::size_t Hash{};
		for (const auto Word :
		     {std::get<0>(Old), std::get<1>(Old), std::uint64_t(std::get<2>(Old)), std::get<3>(Old), std::get<4>(Old)})
		{
			Hash = Hash * 16777619U ^ std::hash<std::uint64_t>{}(Word);
		}
		Check(FInstanceRecordKeyHash{}(Key) == Hash);
		for (const auto Member : {&FInstanceRecordKey::Layout, &FInstanceRecordKey::Scene,
		                          &FInstanceRecordKey::Generation, &FInstanceRecordKey::LocalItem})
		{
			auto Changed = Key;
			++(Changed.*Member);
			Check(!(Changed == Key));
		}
		auto Changed = Key;
		++Changed.Slot;
		Check(!(Changed == Key));
	}
}

void CheckResourceKeys()
{
	const std::array<int, 3> Assets{};
	std::vector<FResourceRequestKey> Keys;
	for (const auto& Asset : Assets)
	{
		for (std::uint64_t Index = 0; Index < 12; ++Index)
		{
			Keys.push_back({&Asset, Index % 2 + 1, Index % 3 ? "Model" : "Mesh", Index / 3});
		}
	}
	const auto Old = [](const FResourceRequestKey& InKey)
	{
		return std::tuple{InKey.Asset, InKey.Version, InKey.Configuration, InKey.Attempt};
	};
	std::map<FResourceRequestKey, unsigned> Entries;
	for (const auto& Key : Keys)
	{
		for (const auto& Other : Keys)
		{
			Check((Key < Other) == (Old(Key) < Old(Other)));
			Check(Key.SameRequest(Other) == (Key.Asset == Other.Asset && Key.Version == Other.Version &&
			                                 Key.Configuration == Other.Configuration));
		}
		Entries.emplace(Key, 0);
	}
	for (auto Request : Keys)
	{
		Request.Attempt = 0;
		std::uint64_t Expected{};
		for (const auto& [Key, Value] : Entries)
		{
			if (Key.SameRequest(Request))
			{
				Expected = std::max(Expected, Key.Attempt + 1);
			}
		}
		for (auto It = Entries.lower_bound(Request); It != Entries.end() && It->first.SameRequest(Request); ++It)
		{
			Request.Attempt = It->first.Attempt + 1;
		}
		Check(Request.Attempt == Expected);
	}
}
} // namespace

int main()
{
	try
	{
		CheckBatchKeys();
		CheckInstanceKeys();
		CheckResourceKeys();
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
