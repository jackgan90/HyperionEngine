#pragma once
#include "Hyperion/Scene/SceneNode.h"
#include <functional>

namespace Hyperion
{
struct FPlaceableObject
{
	std::string Id;
	std::string Label;
	std::vector<std::string> Categories;
	std::optional<FAssetRef> Model;
	std::string Icon;
	// Produces a detached candidate. The document supplies identity, model registration and final transform.
	std::function<FSceneNode()> Create;
};

// Registration is explicit, ordered and independent of any UI or rendering backend.
class FObjectPlacementRegistry
{
public:
	void AddCategory(std::string InId);
	void Add(FPlaceableObject InObject);
	void Remove(std::string_view InId);
	const FPlaceableObject* Find(std::string_view InId) const;
	std::vector<const FPlaceableObject*> Search(std::string_view InCategory, std::string_view InFilter) const;
	const std::vector<std::string>& GetCategories() const;

private:
	std::vector<std::string> Categories;
	std::vector<FPlaceableObject> Objects;
};
} // namespace Hyperion
