#pragma once
#include <string>
#include <string_view>

namespace Hyperion
{
inline constexpr std::string_view AssetPublicationPrefix = ".publish-";

// Exact names only; callers own case folding and any legacy-format exclusions.
bool IsReservedAssetEntry(std::string_view InName);
std::string AssetPublicationLeaseName();
} // namespace Hyperion
