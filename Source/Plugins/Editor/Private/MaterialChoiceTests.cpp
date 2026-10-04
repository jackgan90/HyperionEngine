#include "MaterialChoices.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace Hyperion;

void Check(bool bInCondition)
{
	if (!bInCondition)
	{
		throw std::runtime_error("Option/cache contract check failed");
	}
}

template<class T> void CheckChoices()
{
	auto Choices = MaterialChoices<T>();
	Check(Choices.size() == RecordEnumEntries<T>().size());
	std::reverse(Choices.begin(), Choices.end());
	for (const auto& Entry : RecordEnumEntries<T>())
	{
		const auto Index = PropertyChoiceIndex(Choices, WriteValue(Entry.Value));
		Check(Index && ReadValue<T>(Choices[*Index].Value) == Entry.Value);
		Check(Choices[*Index].Label == MaterialChoiceLabel(Entry.Value, Entry.Name));
	}
}
} // namespace

int main()
{
	try
	{
		CheckChoices<EMaterialAddressMode>();
		CheckChoices<EMaterialSamplerCompare>();
		Check(MaterialChoiceLabel(EMaterialAddressMode::MirrorOnce, "MirrorOnce") == "Mirror once");
		Check(MaterialChoiceLabel(EMaterialSamplerCompare::LessEqual, "LessEqual") == "Less equal");
		Check(MaterialChoiceLabel(EMaterialSamplerCompare::NotEqual, "NotEqual") == "Not equal");
		Check(MaterialChoiceLabel(EMaterialSamplerCompare::GreaterEqual, "GreaterEqual") == "Greater equal");
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
