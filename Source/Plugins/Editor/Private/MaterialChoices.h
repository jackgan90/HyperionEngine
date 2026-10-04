#pragma once
#include "Hyperion/Materials/MaterialAsset.h"

namespace Hyperion
{
inline std::string_view MaterialChoiceLabel(EMaterialAddressMode InValue, std::string_view InName)
{
	return InValue == EMaterialAddressMode::MirrorOnce ? "Mirror once" : InName;
}

inline std::string_view MaterialChoiceLabel(EMaterialSamplerCompare InValue, std::string_view InName)
{
	switch (InValue)
	{
		case EMaterialSamplerCompare::LessEqual:
			return "Less equal";
		case EMaterialSamplerCompare::NotEqual:
			return "Not equal";
		case EMaterialSamplerCompare::GreaterEqual:
			return "Greater equal";
		default:
			return InName;
	}
}

template<class T> std::vector<FPropertyChoice> MaterialChoices()
{
	std::vector<FPropertyChoice> Result;
	for (const auto& Entry : RecordEnumEntries<T>())
	{
		Result.push_back({WriteValue(Entry.Value), std::string(MaterialChoiceLabel(Entry.Value, Entry.Name))});
	}
	return Result;
}
} // namespace Hyperion
