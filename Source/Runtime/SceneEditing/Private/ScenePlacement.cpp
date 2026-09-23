#include "Hyperion/SceneEditing/ScenePlacement.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FPlaceableInfo>()
{
	static const auto Type = MakeRecord<FPlaceableInfo>(
	    "hyperion.placeable.info",
	    {Member("id", &FPlaceableInfo::Id), Member("label", &FPlaceableInfo::Label),
	     Member("categories", &FPlaceableInfo::Categories), Member("unavailable", &FPlaceableInfo::Unavailable)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FPlacementCatalog>()
{
	static const auto Type =
	    MakeRecord<FPlacementCatalog>("hyperion.placeable.catalog", {Member("items", &FPlacementCatalog::Items)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FScenePlacementRequest>()
{
	static const auto Type = MakeRecord<FScenePlacementRequest>(
	    "hyperion.scene.placement",
	    {Member("document", &FScenePlacementRequest::Document, {.bRequired = true}),
	     Member("revision", &FScenePlacementRequest::Revision, {.bRequired = true}),
	     Member("object", &FScenePlacementRequest::Object,
	            {.bRequired = true, .Description = "ID from scene.placement.list."}),
	     Member("position", &FScenePlacementRequest::Position,
	            {.bRequired = true, .Description = "World-space placement pivot, in scene units."})});
	return Type;
}
} // namespace Hyperion
