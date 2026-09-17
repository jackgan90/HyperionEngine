#include "Hyperion/IO/Path.h"

namespace Hyperion
{
bool IsPackagePath(const std::filesystem::path& InPath)
{
	const auto Text = PathToUtf8(InPath);
	return Text.starts_with('/') && !Text.starts_with("//");
}

std::filesystem::path NormalizeFilePath(const std::filesystem::path& InPath)
{
	if (!IsPackagePath(InPath))
	{
		return std::filesystem::absolute(InPath).lexically_normal();
	}
	const auto Text = PathToUtf8(InPath);
	std::filesystem::path Result = "/";
	unsigned Depth = 0;
	for (const auto& Part : InPath.relative_path())
	{
		const auto Name = PathToUtf8(Part);
		if (Name.empty() || Name == ".")
		{
			continue;
		}
		if (Name == "..")
		{
			if (Depth <= 1)
			{
				throw std::invalid_argument("Package path escapes its mount: " + Text);
			}
			Result = Result.parent_path();
			--Depth;
		}
		else
		{
			if (Name.find_first_of(":\\<>\"|?*") != std::string::npos || Name.back() == '.' || Name.back() == ' ')
			{
				throw std::invalid_argument("Invalid package path: " + Text);
			}
			Result /= Part;
			++Depth;
		}
	}
	return Result;
}

std::filesystem::path PathFromUtf8(std::string_view InPath)
{
	return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(InPath.data()), InPath.size()));
}

std::string PathToUtf8(const std::filesystem::path& InPath)
{
	const auto Text = InPath.generic_u8string();
	return {reinterpret_cast<const char*>(Text.data()), Text.size()};
}

std::string PathRelativeToUtf8(const std::filesystem::path& InPath, const std::filesystem::path& InBase)
{
	const auto Relative = InPath.lexically_relative(InBase);
	return PathToUtf8(IsPackagePath(InPath) || Relative.empty() ? InPath : Relative);
}
} // namespace Hyperion
