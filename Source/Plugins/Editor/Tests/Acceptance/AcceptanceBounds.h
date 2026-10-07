#pragma once
#include "../../Private/EditorObservations.h"
#include "Hyperion/Math/Math.h"
#include <compare>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace Hyperion
{
struct FWidgetKey
{
	FWidgetKey(EEditorWidget InWidget) : Widget(InWidget)
	{
	}

	static FWidgetKey WithId(EEditorWidget InWidget, std::string_view InId)
	{
		FWidgetKey Key(InWidget);
		Key.Item = std::string(InId);
		return Key;
	}

	static FWidgetKey WithValue(EEditorWidget InWidget, std::size_t InValue)
	{
		FWidgetKey Key(InWidget);
		Key.Item = InValue;
		return Key;
	}

	auto operator<=>(const FWidgetKey&) const = default;
	EEditorWidget Widget;
	std::variant<std::monostate, std::string, std::size_t> Item;
};

struct FPropertyKey
{
	std::string Component;
	std::string Field;
	auto operator<=>(const FPropertyKey&) const = default;
};

class FAcceptanceBounds
{
public:
	void Set(FWidgetKey InKey, FVec4 InBounds)
	{
		Widgets.insert_or_assign(std::move(InKey), InBounds);
	}

	void Set(FPropertyKey InKey, FVec4 InBounds)
	{
		Properties.insert_or_assign(std::move(InKey), InBounds);
	}

	FVec4 Require(const FWidgetKey& InKey) const
	{
		return Widgets.at(InKey);
	}

	FVec4 Require(const FPropertyKey& InKey) const
	{
		return Properties.at(InKey);
	}

	// Polling an undrawn control keeps the old empty-rectangle wait behavior without creating an observation.
	FVec4 FindOrEmpty(const FWidgetKey& InKey) const
	{
		const auto Found = Widgets.find(InKey);
		return Found == Widgets.end() ? FVec4{} : Found->second;
	}

	FVec4 FindOrEmpty(const FPropertyKey& InKey) const
	{
		const auto Found = Properties.find(InKey);
		return Found == Properties.end() ? FVec4{} : Found->second;
	}

	bool Contains(const FWidgetKey& InKey) const
	{
		return Widgets.contains(InKey);
	}

	bool Contains(const FPropertyKey& InKey) const
	{
		return Properties.contains(InKey);
	}

	void Clear()
	{
		Widgets.clear();
		Properties.clear();
	}

private:
	std::map<FWidgetKey, FVec4> Widgets;
	std::map<FPropertyKey, FVec4> Properties;
};
} // namespace Hyperion
