#include "Hyperion/SceneEditing/ScenePlacement.h"
#include "SceneOperations.h"

namespace Hyperion
{
void RegisterPlacementOperations(FOperationCatalog& InCatalog, IScenePlacement* InPlacement)
{
	FOperationInfo Info;
	Info.Id = "scene.placement.list";
	Info.Owner = "automation-scene";
	Info.Summary = "List the target's registered placeable objects";
	Info.Description = "Shares the GUI placement registry. Preparing resources are requested by scene.placement.place.";
	Info.bReadOnly = true;
	Info.Effects = "Reads the placement catalog.";
	Info.Completion = "Current Main snapshot.";
	Info.Unavailable = InPlacement ? "" : "This target has no placement provider.";
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FPlacementCatalog>(Info,
	                                                                       [InPlacement](const auto&)
	                                                                       {
		                                                                       return InPlacement->PlacementCatalog();
	                                                                       }));
	Info.Id = "scene.placement.place";
	Info.Summary = "Prepare and place a registered object";
	Info.Description =
	    "Prepare the same resources and commit the same placement/history action as GUI placement. Object pivot uses "
	    "explicit world coordinates. Concurrent scene replacement or edits reject the stale request before commit.";
	Info.bReadOnly = false;
	Info.Effects = "Creates and selects one node in shared history. Save explicitly.";
	Info.Completion = "Required resources prepared and scene node committed. No disk write.";
	const FScenePlacementRequest Example{"document-from-scene.info", 1, "Cube", {}};
	Info.Example = WriteRecordWire(RecordType<FScenePlacementRequest>(), &Example);
	InCatalog.Register(MakeAsyncOperation<FScenePlacementRequest, FSceneNodeInfo>(
	    Info,
	    [InPlacement](const auto& InRequest)
	    {
		    return TPendingOperation<FSceneNodeInfo>{[InPlacement, InRequest]()
		                                             {
			                                             return InPlacement->PlaceObject(InRequest);
		                                             }};
	    }));
	Info.Id = "scene.placement.place_model";
	Info.Summary = "Prepare and place a native model asset";
	Info.Description =
	    "Shares native model preparation and the creation/history transaction with Content Browser viewport drops. "
	    "Creates one node preserving model instances and materials at an explicit world pivot. "
	    "Concurrent scene replacement or edits reject the stale request before commit.";
	const FSceneModelPlacementRequest ModelExample{
	    "document-from-scene.info", 1, {"", "/Game/Models/Example.hasset", RecordType<FModelAsset>().Id, ""}, {}};
	Info.Example = WriteRecordWire(RecordType<FSceneModelPlacementRequest>(), &ModelExample);
	InCatalog.Register(MakeAsyncOperation<FSceneModelPlacementRequest, FSceneNodeInfo>(
	    Info,
	    [InPlacement](const auto& InRequest)
	    {
		    return TPendingOperation<FSceneNodeInfo>{[InPlacement, InRequest]()
		                                             {
			                                             return InPlacement->PlaceModel(InRequest);
		                                             }};
	    }));
}
} // namespace Hyperion
