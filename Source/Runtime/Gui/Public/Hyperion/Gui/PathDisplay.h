#pragma once
#include <string>
#include <string_view>

namespace Hyperion
{
// Presentation only: package identities remain absolute in document and asset data.
struct FGuiPathDisplay
{
	std::string Root;
	std::string Text(std::string_view InText) const;
	std::string Value(std::string_view InPath) const;
	std::string Resolve(std::string_view InPath) const;
};
} // namespace Hyperion
