#include "Hyperion/Reflection/Json.h"
#include <cmath>
#include <nlohmann/json.hpp>
#include <set>

namespace Hyperion
{
namespace
{
using FJson = nlohmann::json;

// Check before copying strings/containers into the vendor DOM. Escaping can only increase the
// final size by a bounded factor; WriteJson checks the exact encoded byte count afterwards.
void CheckOutput(const FArchiveNode& InValue, std::size_t& OutBytes, std::size_t& OutNodes, unsigned InDepth,
                 const FJsonLimits& InLimits)
{
	if (++OutNodes > InLimits.MaxNodes || InDepth > InLimits.MaxDepth)
	{
		throw std::invalid_argument("JSON exceeds node/depth limit");
	}
	auto Add = [&](std::size_t InBytes)
	{
		if (InBytes > InLimits.MaxBytes - OutBytes)
		{
			throw std::invalid_argument("JSON exceeds byte limit");
		}
		OutBytes += InBytes;
	};
	if (const auto* Text = std::get_if<std::string>(&InValue.Value))
	{
		Add(Text->size());
	}
	else if (const auto* Fields = std::get_if<FArchiveNode::FObject>(&InValue.Value))
	{
		for (const auto& [Key, Value] : *Fields)
		{
			Add(Key.size());
			CheckOutput(Value, OutBytes, OutNodes, InDepth + 1, InLimits);
		}
	}
	else if (const auto* Values = std::get_if<FArchiveNode::FArray>(&InValue.Value))
	{
		for (const auto& Value : *Values)
		{
			CheckOutput(Value, OutBytes, OutNodes, InDepth + 1, InLimits);
		}
	}
	else if (std::holds_alternative<FBulkData>(InValue.Value))
	{
		throw std::invalid_argument("Bulk data is not a plain JSON value");
	}
}

void CheckBudget(std::size_t& OutNodes, unsigned InDepth, const FJsonLimits& InLimits)
{
	if (++OutNodes > InLimits.MaxNodes || InDepth > InLimits.MaxDepth)
	{
		throw std::invalid_argument("JSON exceeds node/depth limit");
	}
}

FArchiveNode Decode(const FJson& InValue, std::size_t& OutNodes, unsigned InDepth, const FJsonLimits& InLimits)
{
	CheckBudget(OutNodes, InDepth, InLimits);
	if (InValue.is_object())
	{
		FArchiveNode::FObject Result;
		for (const auto& [Key, Value] : InValue.items())
		{
			Result.emplace(Key, Decode(Value, OutNodes, InDepth + 1, InLimits));
		}
		return FArchiveNode(std::move(Result));
	}
	if (InValue.is_array())
	{
		FArchiveNode::FArray Result;
		for (const auto& Value : InValue)
		{
			Result.push_back(Decode(Value, OutNodes, InDepth + 1, InLimits));
		}
		return FArchiveNode(std::move(Result));
	}
	if (InValue.is_null())
	{
		return FArchiveNode(std::monostate{});
	}
	if (InValue.is_boolean())
	{
		return FArchiveNode(InValue.get<bool>());
	}
	if (InValue.is_string())
	{
		return FArchiveNode(InValue.get<std::string>());
	}
	if (InValue.is_number_unsigned())
	{
		return FArchiveNode(InValue.get<std::uint64_t>());
	}
	if (InValue.is_number_integer())
	{
		return FArchiveNode(InValue.get<std::int64_t>());
	}
	const double Number = InValue.get<double>();
	if (!std::isfinite(Number))
	{
		throw std::invalid_argument("Non-finite JSON number");
	}
	return FArchiveNode(Number);
}

FJson Encode(const FArchiveNode& InValue, std::size_t& OutNodes, unsigned InDepth, const FJsonLimits& InLimits)
{
	CheckBudget(OutNodes, InDepth, InLimits);
	return std::visit(
	    [&](const auto& InItem) -> FJson
	    {
		    using FItem = std::decay_t<decltype(InItem)>;
		    if constexpr (std::is_same_v<FItem, FArchiveNode::FObject>)
		    {
			    FJson Result = FJson::object();
			    for (const auto& [Key, Value] : InItem)
			    {
				    Result[Key] = Encode(Value, OutNodes, InDepth + 1, InLimits);
			    }
			    return Result;
		    }
		    else if constexpr (std::is_same_v<FItem, FArchiveNode::FArray>)
		    {
			    FJson Result = FJson::array();
			    for (const auto& Value : InItem)
			    {
				    Result.push_back(Encode(Value, OutNodes, InDepth + 1, InLimits));
			    }
			    return Result;
		    }
		    else if constexpr (std::is_same_v<FItem, FBulkData>)
		    {
			    throw std::invalid_argument("Bulk data is not a plain JSON value");
		    }
		    else if constexpr (std::is_same_v<FItem, std::monostate>)
		    {
			    return nullptr;
		    }
		    else
		    {
			    if constexpr (std::is_same_v<FItem, double>)
			    {
				    if (!std::isfinite(InItem))
				    {
					    throw std::invalid_argument("Non-finite JSON number");
				    }
			    }
			    return InItem;
		    }
	    },
	    InValue.Value);
}
} // namespace

FArchiveNode ParseJson(std::string_view InText, FJsonLimits InLimits)
{
	if (InText.size() > InLimits.MaxBytes)
	{
		throw std::invalid_argument("JSON exceeds byte limit");
	}
	std::vector<std::set<std::string>> Keys;
	std::size_t Nodes{};
	auto Callback = [&](int InDepth, FJson::parse_event_t InEvent, FJson& InValue)
	{
		if (InDepth < 0 || static_cast<unsigned>(InDepth) > InLimits.MaxDepth || ++Nodes > InLimits.MaxNodes * 3)
		{
			throw std::invalid_argument("JSON exceeds node/depth limit");
		}
		if (InEvent == FJson::parse_event_t::object_start)
		{
			Keys.emplace_back();
		}
		else if (InEvent == FJson::parse_event_t::object_end)
		{
			Keys.pop_back();
		}
		else if (InEvent == FJson::parse_event_t::key && !Keys.back().insert(InValue.get<std::string>()).second)
		{
			throw std::invalid_argument("Duplicate JSON key: " + InValue.get<std::string>());
		}
		return true;
	};
	try
	{
		const auto Parsed = FJson::parse(InText, Callback);
		Nodes = 0;
		return Decode(Parsed, Nodes, 0, InLimits);
	}
	catch (const FJson::exception& Error)
	{
		throw std::invalid_argument(Error.what());
	}
}

std::string WriteJson(const FArchiveNode& InValue, FJsonLimits InLimits)
{
	std::size_t Nodes{};
	std::size_t Bytes{};
	CheckOutput(InValue, Bytes, Nodes, 0, InLimits);
	Nodes = 0;
	const auto Result = Encode(InValue, Nodes, 0, InLimits).dump();
	if (Result.size() > InLimits.MaxBytes)
	{
		throw std::invalid_argument("JSON exceeds byte limit");
	}
	return Result;
}
} // namespace Hyperion
