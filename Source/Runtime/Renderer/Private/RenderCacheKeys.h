#pragma once
#include "Hyperion/Math/DepthConvention.h"
#include <cstdint>
#include <functional>
#include <string>
#include <tuple>

namespace Hyperion
{
struct FBatchItemKey
{
	std::uint64_t View{};
	std::string Usage;
	std::uint64_t Scene{};
	std::uint32_t Slot{};
	std::uint64_t Generation{};
	std::uint64_t LocalItem{};
	EDepthConvention DepthConvention = EDepthConvention::Standard;

	auto operator<=>(const FBatchItemKey&) const = default;

	static FBatchItemKey ViewBegin(std::uint64_t InView, const std::string& InUsage)
	{
		return {.View = InView, .Usage = InUsage};
	}

	bool MatchesView(std::uint64_t InView, const std::string& InUsage) const
	{
		return View == InView && Usage == InUsage;
	}
};

struct FBatchItemKeyHash
{
	std::size_t operator()(const FBatchItemKey& InKey) const
	{
		std::size_t Hash = std::hash<std::string>{}(InKey.Usage);
		for (const auto Word : {InKey.View, InKey.Scene, std::uint64_t(InKey.Slot), InKey.Generation, InKey.LocalItem,
		                        std::uint64_t(InKey.DepthConvention)})
		{
			Hash = Hash * 16777619U ^ std::hash<std::uint64_t>{}(Word);
		}
		return Hash;
	}
};

struct FInstanceRecordKey
{
	std::uint64_t Layout{};
	std::uint64_t Scene{};
	std::uint32_t Slot{};
	std::uint64_t Generation{};
	std::uint64_t LocalItem{};
	bool operator==(const FInstanceRecordKey&) const = default;
};

struct FInstanceRecordKeyHash
{
	std::size_t operator()(const FInstanceRecordKey& InKey) const
	{
		std::size_t Hash{};
		for (const auto Word :
		     {InKey.Layout, InKey.Scene, std::uint64_t(InKey.Slot), InKey.Generation, InKey.LocalItem})
		{
			Hash = Hash * 16777619U ^ std::hash<std::uint64_t>{}(Word);
		}
		return Hash;
	}
};

struct FResourceRequestKey
{
	const void* Asset{};
	std::uint64_t Version{};
	std::string Configuration;
	std::uint64_t Attempt{};

	bool operator<(const FResourceRequestKey& InOther) const
	{
		// Preserve the tuple's pointer ordering as well as its lexicographic field order.
		return std::tie(Asset, Version, Configuration, Attempt) <
		       std::tie(InOther.Asset, InOther.Version, InOther.Configuration, InOther.Attempt);
	}

	bool SameRequest(const FResourceRequestKey& InOther) const
	{
		return Asset == InOther.Asset && Version == InOther.Version && Configuration == InOther.Configuration;
	}
};
} // namespace Hyperion
