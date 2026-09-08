#include "Hyperion/Renderer/MaterialInputValues.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{
std::size_t TypeStorageBytes(const FMaterialParameterType& InType)
{
	std::size_t Bytes = InType.Members.capacity() * sizeof(FMaterialParameterType) +
	                    InType.MemberNames.capacity() * sizeof(std::string);
	for (const auto& Name : InType.MemberNames)
	{
		Bytes += Name.capacity();
	}
	for (const auto& Member : InType.Members)
	{
		Bytes += TypeStorageBytes(Member);
	}
	return Bytes;
}
} // namespace

std::size_t MaterialValueStorageBytes(const FMaterialValue& InValue)
{
	std::size_t Bytes = sizeof(FMaterialValue) + TypeStorageBytes(InValue.Type) +
	                    InValue.Words.capacity() * sizeof(std::uint32_t) +
	                    InValue.Elements.capacity() * sizeof(FMaterialValue);
	for (const auto& Element : InValue.Elements)
	{
		Bytes += MaterialValueStorageBytes(Element) - sizeof(Element);
	}
	return Bytes;
}

FMaterialInputValues::FMaterialInputValues(FMaterialParameterValues InValues)
{
	std::sort(InValues.begin(), InValues.end(),
	          [](const FMaterialParameterEntry& InA, const FMaterialParameterEntry& InB)
	          {
		          return InA.Name < InB.Name;
	          });
	std::string_view Previous;
	for (const auto& Entry : InValues)
	{
		if (Entry.Name.empty() || Entry.Name == Previous)
		{
			throw std::invalid_argument("Duplicate or empty material provider input");
		}
		Entry.Value.Validate();
		Previous = Entry.Name;
	}
	if (!InValues.empty())
	{
		StorageBytes = sizeof(FMaterialParameterValues) + InValues.capacity() * sizeof(FMaterialParameterEntry);
		for (const auto& Entry : InValues)
		{
			StorageBytes += Entry.Name.capacity() + MaterialValueStorageBytes(Entry.Value) - sizeof(Entry.Value);
		}
		Storage = std::make_shared<const FMaterialParameterValues>(std::move(InValues));
	}
}

const FMaterialParameterValues& FMaterialInputValues::Get() const
{
	static const FMaterialParameterValues Empty;
	return Storage ? *Storage : Empty;
}
} // namespace Hyperion
