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
			                                             try
			                                             {
				                                             return InPlacement->PlaceObject(InRequest);
			                                             }
			                                             catch (const FSceneEditError& Error)
			                                             {
				                                             throw FAutomationError(Error.Code, Error.what());
			                                             }
		                                             }};
	    }));
}
} // namespace Hyperion
