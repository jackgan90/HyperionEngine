#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include <nlohmann/json.hpp>
#include <set>

namespace Hyperion
{
namespace
{
using FJson = nlohmann::json;

template<class T> FArchiveNode DecodeBulk(const FJson& InData)
{
	std::vector<T> Values;
	Values.reserve(InData.size());
	for (const auto& Item : InData)
	{
		if constexpr (std::is_floating_point_v<T>)
		{
			if (!Item.is_number() || !std::isfinite(Item.get<double>()) ||
			    std::abs(Item.get<double>()) > std::numeric_limits<T>::max())
			{
				throw std::invalid_argument("JSON bulk floating value out of range");
			}
			Values.push_back(static_cast<T>(Item.get<double>()));
		}
		else
		{
			if (Item.is_number_unsigned())
			{
				Values.push_back(ReadInteger<T>(WriteValue(Item.get<std::uint64_t>())));
			}
			else if (Item.is_number_integer())
			{
				Values.push_back(ReadInteger<T>(WriteValue(Item.get<std::int64_t>())));
			}
			else
			{
				throw std::invalid_argument("JSON integer bulk requires integer elements");
			}
		}
	}
	return WriteValue(Values);
}

FArchiveNode DecodeNode(const FJson& InNode)
{
	if (InNode.is_null())
	{
		return FArchiveNode(std::monostate{});
	}
	if (InNode.is_boolean())
	{
		return WriteValue(InNode.get<bool>());
	}
	if (InNode.is_number_unsigned())
	{
		return WriteValue(InNode.get<std::uint64_t>());
	}
	if (InNode.is_number_integer())
	{
		return WriteValue(InNode.get<std::int64_t>());
	}
	if (InNode.is_number_float())
	{
		return WriteValue(InNode.get<double>());
	}
	if (InNode.is_string())
	{
		return WriteValue(InNode.get<std::string>());
	}
	if (InNode.is_array())
	{
		FArchiveNode::FArray Values;
		for (const auto& Value : InNode)
		{
			Values.push_back(DecodeNode(Value));
		}
		return FArchiveNode(std::move(Values));
	}
	if (InNode.contains("$bulk"))
	{
		if (InNode.size() != 2 || !InNode.at("data").is_array())
		{
			throw std::invalid_argument("Invalid tagged bulk JSON");
		}
		const auto Element = InNode.at("$bulk").get<std::string>();
		if (Element == "u8")
		{
			return DecodeBulk<std::uint8_t>(InNode.at("data"));
		}
		if (Element == "i8")
		{
			return DecodeBulk<std::int8_t>(InNode.at("data"));
		}
		if (Element == "u16")
		{
			return DecodeBulk<std::uint16_t>(InNode.at("data"));
		}
		if (Element == "i16")
		{
			return DecodeBulk<std::int16_t>(InNode.at("data"));
		}
		if (Element == "u32")
		{
			return DecodeBulk<std::uint32_t>(InNode.at("data"));
		}
		if (Element == "i32")
		{
			return DecodeBulk<std::int32_t>(InNode.at("data"));
		}
		if (Element == "u64")
		{
			return DecodeBulk<std::uint64_t>(InNode.at("data"));
		}
		if (Element == "i64")
		{
			return DecodeBulk<std::int64_t>(InNode.at("data"));
		}
		if (Element == "f32")
		{
			return DecodeBulk<float>(InNode.at("data"));
		}
		if (Element == "f64")
		{
			return DecodeBulk<double>(InNode.at("data"));
		}
		throw std::invalid_argument("Unsupported bulk element: " + Element);
	}
	FArchiveNode::FObject Values;
	for (const auto& [Key, Value] : InNode.items())
	{
		Values.emplace(Key, DecodeNode(Value));
	}
	return FArchiveNode(std::move(Values));
}

template<class T> FJson BulkJson(const FArchiveNode& InNode)
{
	return ReadBulk<std::vector<T>>(InNode);
}

FJson EncodeBulk(const FArchiveNode& InNode)
{
	const auto& Element = std::get<FBulkData>(InNode.Value).Element;
	FJson Data;
	if (Element == "u8")
	{
		Data = BulkJson<std::uint8_t>(InNode);
	}
	else if (Element == "i8")
	{
		Data = BulkJson<std::int8_t>(InNode);
	}
	else if (Element == "u16")
	{
		Data = BulkJson<std::uint16_t>(InNode);
	}
	else if (Element == "i16")
	{
		Data = BulkJson<std::int16_t>(InNode);
	}
	else if (Element == "u32")
	{
		Data = BulkJson<std::uint32_t>(InNode);
	}
	else if (Element == "i32")
	{
		Data = BulkJson<std::int32_t>(InNode);
	}
	else if (Element == "u64")
	{
		Data = BulkJson<std::uint64_t>(InNode);
	}
	else if (Element == "i64")
	{
		Data = BulkJson<std::int64_t>(InNode);
	}
	else if (Element == "f32")
	{
		Data = BulkJson<float>(InNode);
	}
	else if (Element == "f64")
	{
		Data = BulkJson<double>(InNode);
	}
	else
	{
		throw std::invalid_argument("Unsupported bulk element: " + Element);
	}
	return FJson{{"$bulk", Element}, {"data", std::move(Data)}};
}

FJson EncodeNode(const FArchiveNode& InNode)
{
	return std::visit(
	    [&](const auto& InValue) -> FJson
	    {
		    using FType = std::decay_t<decltype(InValue)>;
		    if constexpr (std::is_same_v<FType, std::monostate>)
		    {
			    return nullptr;
		    }
		    else if constexpr (std::is_same_v<FType, FBulkData>)
		    {
			    return EncodeBulk(InNode);
		    }
		    else if constexpr (std::is_same_v<FType, FArchiveNode::FArray>)
		    {
			    FJson Result = FJson::array();
			    for (const auto& Value : InValue)
			    {
				    Result.push_back(EncodeNode(Value));
			    }
			    return Result;
		    }
		    else if constexpr (std::is_same_v<FType, FArchiveNode::FObject>)
		    {
			    FJson Result = FJson::object();
			    for (const auto& [Key, Value] : InValue)
			    {
				    Result[Key] = EncodeNode(Value);
			    }
			    return Result;
		    }
		    else
		    {
			    return InValue;
		    }
	    },
	    InNode.Value);
}
} // namespace

FArchiveNode DecodeAssetSourceJson(std::string_view InText)
{
	if (InText.size() > 64U * 1024 * 1024)
	{
		throw std::invalid_argument("Asset JSON source exceeds 64 MiB");
	}
	std::size_t Nodes{};
	std::map<int, std::set<std::string>> Keys;
	const auto Callback = [&](int InDepth, FJson::parse_event_t InEvent, FJson& InValue)
	{
		if (InDepth > 64 || ++Nodes > 1000000)
		{
			throw std::invalid_argument("Asset JSON nesting/node budget exceeded");
		}
		if (InEvent == FJson::parse_event_t::object_start)
		{
			Keys[InDepth + 1].clear();
		}
		if (InEvent == FJson::parse_event_t::key && !Keys[InDepth].insert(InValue.get<std::string>()).second)
		{
			throw std::invalid_argument("Duplicate asset JSON key");
		}
		return true;
	};
	return DecodeNode(FJson::parse(InText, Callback));
}

std::string EncodeAssetSourceJson(const FArchiveNode& InNode)
{
	// Apply the normal archive budgets before recursively exporting untrusted records.
	(void)HashArchive(InNode);
	return EncodeNode(InNode).dump(2) + "\n";
}

void RegisterMaterialImporters(FAssetImportService& InImports)
{
	for (const auto* Type : {&RecordType<FMaterialAsset>(), &RecordType<FTextureAsset>()})
	{
		InImports.Register({"hyperion.native-" + Type->Id,
		                    1,
		                    Type,
		                    {".hasset"},
		                    [Type](FAssetImportContext& InContext)
		                    {
			                    return ReadRecord(*Type, DecodeAsset(InContext.Bytes).Object);
		                    }});
		InImports.Register({"hyperion.json-" + Type->Id,
		                    1,
		                    Type,
		                    {".json"},
		                    [Type](FAssetImportContext& InContext)
		                    {
			                    const std::string_view Text(reinterpret_cast<const char*>(InContext.Bytes->data()),
			                                                InContext.Bytes->size());
			                    return ReadRecord(*Type, DecodeAssetSourceJson(Text));
		                    }});
	}
}
} // namespace Hyperion
