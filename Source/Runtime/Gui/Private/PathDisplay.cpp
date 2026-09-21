#include "Hyperion/Gui/PathDisplay.h"

namespace Hyperion
{
std::string FGuiPathDisplay::Text(std::string_view InText) const
{
	std::string Result(InText);
	if (Root.empty())
	{
		return Result;
	}
	const auto Prefix = Root + "/";
	std::size_t Position{};
	while ((Position = Result.find(Prefix, Position)) != std::string::npos)
	{
		// Do not rewrite a physical path or a longer path component containing this spelling.
		if (!Position || std::string_view(" \t\r\n\"'([{=|<").find(Result[Position - 1]) != std::string_view::npos)
		{
			Result.erase(Position, Prefix.size());
		}
		else
		{
			Position += Prefix.size();
		}
	}
	return Result;
}

std::string FGuiPathDisplay::Value(std::string_view InPath) const
{
	if (!Root.empty() && InPath == Root)
	{
		return {};
	}
	const auto Prefix = Root + "/";
	return std::string(!Root.empty() && InPath.starts_with(Prefix) ? InPath.substr(Prefix.size()) : InPath);
}

std::string FGuiPathDisplay::Resolve(std::string_view InPath) const
{
	if (Root.empty() || InPath.empty() || InPath.front() == '/' || InPath.front() == '\\' ||
	    (InPath.size() > 1 && InPath[1] == ':'))
	{
		return std::string(InPath);
	}
	return Root + "/" + std::string(InPath);
}
} // namespace Hyperion
