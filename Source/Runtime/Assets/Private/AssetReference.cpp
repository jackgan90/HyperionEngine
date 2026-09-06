#include "Hyperion/Assets/Assets.h"

namespace Hyperion
{
const FTypeDescriptor& AssetReferenceType()
{
	static const FTypeDescriptor Type{"hyperion.asset-reference",
	                                  1,
	                                  {{"id", "Asset ID", EPropertyKind::String, 0, 0, true,
	                                    [](const void* InP) -> FValue
	                                    {
		                                    return static_cast<const FAssetReference*>(InP)->Id;
	                                    },
	                                    [](void* InP, const FValue& InV)
	                                    {
		                                    static_cast<FAssetReference*>(InP)->Id = std::get<std::string>(InV);
	                                    }},
	                                   {"source", "Source", EPropertyKind::String, 0, 0, true,
	                                    [](const void* InP) -> FValue
	                                    {
		                                    return static_cast<const FAssetReference*>(InP)->Source;
	                                    },
	                                    [](void* InP, const FValue& InV)
	                                    {
		                                    static_cast<FAssetReference*>(InP)->Source = std::get<std::string>(InV);
	                                    }}}};
	return Type;
}
} // namespace Hyperion
