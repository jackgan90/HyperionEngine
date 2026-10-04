#include "Hyperion/AssetImport/AssetSourceJson.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include <nlohmann/json.hpp>

namespace Hyperion
{
namespace
{
using FJson = nlohmann::json;

FJson EncodeBulk(const FArchiveNode& InNode)
{
	const auto Element = std::get<FBulkData>(InNode.Value).Element;
	auto Data = VisitBulkElement(Element,
	                             [&]<class T>() -> FJson
	                             {
		                             return ReadBulk<std::vector<T>>(InNode);
	                             });
	return FJson{{"$bulk", GetBulkElementInfo(Element).WireName}, {"data", std::move(Data)}};
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
