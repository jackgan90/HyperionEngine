#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace Hyperion
{
inline bool MatchesEditorFilter(std::string InText, std::string InFilter)
{
	const auto Lower = [](unsigned char InValue)
	{
		return static_cast<char>(std::tolower(InValue));
	};
	std::transform(InText.begin(), InText.end(), InText.begin(), Lower);
	std::transform(InFilter.begin(), InFilter.end(), InFilter.begin(), Lower);
	return InText.find(InFilter) != std::string::npos;
}
} // namespace Hyperion
