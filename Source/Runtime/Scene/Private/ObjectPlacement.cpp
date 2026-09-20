#include "Hyperion/Scene/ObjectPlacement.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace Hyperion
{
namespace
{
std::string Lower(std::string_view InText)
{
	std::string Result(InText);
	std::transform(Result.begin(), Result.end(), Result.begin(),
	               [](unsigned char InValue)
	               {
		               return static_cast<char>(std::tolower(InValue));
	               });
	return Result;
}
} // namespace

void FObjectPlacementRegistry::AddCategory(std::string InId)
{
	if (InId.empty() || InId == "All" || std::find(Categories.begin(), Categories.end(), InId) != Categories.end())
	{
		throw std::invalid_argument("Invalid or duplicate placement category: " + InId);
	}
	Categories.push_back(std::move(InId));
}

void FObjectPlacementRegistry::Add(FPlaceableObject InObject)
{
	if (InObject.Id.empty() || InObject.Label.empty() || !InObject.Create || Find(InObject.Id) ||
	    InObject.Categories.empty())
	{
		throw std::invalid_argument("Invalid or duplicate placeable object: " + InObject.Id);
	}
	if (InObject.Model && (InObject.Model->TypeId != RecordType<FModelAsset>().Id ||
	                       (InObject.Model->Path.empty() && InObject.Model->Id.empty())))
	{
		throw std::invalid_argument("Placeable model requires a native model reference");
	}
	std::set<std::string> Seen;
	for (const auto& Category : InObject.Categories)
	{
		if (!Seen.insert(Category).second ||
		    std::find(Categories.begin(), Categories.end(), Category) == Categories.end())
		{
			throw std::invalid_argument("Unknown or duplicate placement category: " + Category);
		}
	}
	Objects.push_back(std::move(InObject));
}

void FObjectPlacementRegistry::Remove(std::string_view InId)
{
	std::erase_if(Objects,
	              [&](const auto& InObject)
	              {
		              return InObject.Id == InId;
	              });
}

const FPlaceableObject* FObjectPlacementRegistry::Find(std::string_view InId) const
{
	const auto It = std::find_if(Objects.begin(), Objects.end(),
	                             [&](const auto& InObject)
	                             {
		                             return InObject.Id == InId;
	                             });
	return It == Objects.end() ? nullptr : &*It;
}

std::vector<const FPlaceableObject*> FObjectPlacementRegistry::Search(std::string_view InCategory,
                                                                      std::string_view InFilter) const
{
	std::vector<const FPlaceableObject*> Result;
	const auto Filter = Lower(InFilter);
	for (const auto& Object : Objects)
	{
		if ((InCategory.empty() || InCategory == "All" ||
		     std::find(Object.Categories.begin(), Object.Categories.end(), InCategory) != Object.Categories.end()) &&
		    (Lower(Object.Label).find(Filter) != std::string::npos ||
		     Lower(Object.Id).find(Filter) != std::string::npos))
		{
			Result.push_back(&Object);
		}
	}
	return Result;
}

const std::vector<std::string>& FObjectPlacementRegistry::GetCategories() const
{
	return Categories;
}
} // namespace Hyperion
