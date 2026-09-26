#include "Hyperion/AssetImport/AssetSourceJson.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include <nlohmann/json.hpp>

namespace Hyperion
{
namespace
{
using FJson = nlohmann::json;

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

std::string EncodeAssetSourceJson(const FArchiveNode& InNode)
{
	// Apply the normal archive budgets before recursively exporting untrusted records.
	(void)HashArchive(InNode);
	return EncodeNode(InNode).dump(2) + "\n";
}

} // namespace Hyperion
