#include "Hyperion/IO/Path.h"

namespace Hyperion
{
std::filesystem::path PathFromUtf8(std::string_view InPath)
{
	return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(InPath.data()), InPath.size()));
}

std::string PathToUtf8(const std::filesystem::path& InPath)
{
	const auto Text = InPath.generic_u8string();
	return {reinterpret_cast<const char*>(Text.data()), Text.size()};
}
} // namespace Hyperion
