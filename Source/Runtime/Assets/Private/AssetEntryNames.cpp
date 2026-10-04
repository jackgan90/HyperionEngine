#include "Hyperion/Assets/AssetEntryNames.h"

namespace Hyperion
{
bool IsReservedAssetEntry(std::string_view InName)
{
	return InName == ".git" || InName == ".cache" || InName.starts_with(AssetPublicationPrefix);
}

std::string AssetPublicationLeaseName()
{
	return std::string(AssetPublicationPrefix) + "library";
}
} // namespace Hyperion
