#pragma once
#include "Hyperion/SceneEditing/SceneRequests.h"

namespace Hyperion
{
struct FPlaceableInfo
{
	std::string Id;
	std::string Label;
	std::vector<std::string> Categories;
	std::string Unavailable;
};

struct FPlacementCatalog
{
	std::vector<FPlaceableInfo> Items;
};

struct FScenePlacementRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::string Object;
	FVec3 Position;
};

class IScenePlacement
{
public:
	virtual ~IScenePlacement() = default;
	virtual FPlacementCatalog PlacementCatalog() const = 0;
	// Main polling. The host prepares resources and commits through its shared document once ready.
	virtual std::optional<FSceneNodeInfo> PlaceObject(const FScenePlacementRequest& InRequest) = 0;
};

template<> const FRecordDescriptor& RecordType<FPlaceableInfo>();
template<> const FRecordDescriptor& RecordType<FPlacementCatalog>();
template<> const FRecordDescriptor& RecordType<FScenePlacementRequest>();
} // namespace Hyperion
