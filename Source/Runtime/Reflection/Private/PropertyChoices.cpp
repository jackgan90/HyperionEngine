#include "Hyperion/Reflection/Record.h"

namespace Hyperion
{
bool FPropertyChoice::operator==(const FPropertyChoice& InOther) const
{
	return Label == InOther.Label && EqualInspectionValue(Value, InOther.Value);
}

std::optional<std::size_t> PropertyChoiceIndex(std::span<const FPropertyChoice> InChoices, const FArchiveNode& InValue)
{
	for (std::size_t Index = 0; Index < InChoices.size(); ++Index)
	{
		if (EqualInspectionValue(InChoices[Index].Value, InValue))
		{
			return Index;
		}
	}
	return {};
}

std::vector<std::string> PropertyChoiceLabels(std::span<const FPropertyChoice> InChoices)
{
	std::vector<std::string> Result;
	Result.reserve(InChoices.size());
	for (const auto& Choice : InChoices)
	{
		Result.push_back(Choice.Label);
	}
	return Result;
}
} // namespace Hyperion
