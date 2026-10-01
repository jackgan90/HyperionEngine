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
	for (auto& Entry : InValues)
	{
		Entry = {Entry.GetSemantic(), std::move(Entry.Value)};
	}
	std::sort(InValues.begin(), InValues.end(),
	          [](const FMaterialParameterEntry& InA, const FMaterialParameterEntry& InB)
	          {
		          return InA.Semantic < InB.Semantic;
	          });
	FMaterialSemanticId Previous;
	for (const auto& Entry : InValues)
	{
		if (Entry.Semantic.IsEmpty() || Entry.Semantic == Previous)
		{
			throw std::invalid_argument("Duplicate or empty material provider input");
		}
		Entry.Value.Validate();
		Previous = Entry.Semantic;
	}
	if (!InValues.empty())
	{
		auto Lookup = std::make_shared<FSemanticLookup>();
		for (std::size_t Index = 0; Index < InValues.size(); ++Index)
		{
			if (!Lookup->emplace(InValues[Index].Semantic, Index).second)
			{
				throw std::invalid_argument("Duplicate material provider semantic identity");
			}
		}
		SemanticLookup = std::move(Lookup);
		StorageBytes = sizeof(FMaterialParameterValues) + InValues.capacity() * sizeof(FMaterialParameterEntry);
		StorageBytes +=
		    sizeof(FSemanticLookup) + InValues.size() * (sizeof(FSemanticLookup::value_type) + 3 * sizeof(void*));
		for (const auto& Entry : InValues)
		{
			StorageBytes += Entry.Name.capacity() + Entry.Semantic.GetStorageBytes() +
			                MaterialValueStorageBytes(Entry.Value) - sizeof(Entry.Value);
		}
		Storage = std::make_shared<const FMaterialParameterValues>(std::move(InValues));
	}
}

const FMaterialParameterValues& FMaterialInputValues::Get() const
{
	static const FMaterialParameterValues Empty;
	return Storage ? *Storage : Empty;
}

const FMaterialValue* FMaterialInputValues::Find(FMaterialSemanticId InSemantic) const
{
	if (!SemanticLookup)
	{
		return nullptr;
	}
	const auto Found = SemanticLookup->find(InSemantic);
	return Found != SemanticLookup->end() ? &Storage->at(Found->second).Value : nullptr;
}
} // namespace Hyperion
