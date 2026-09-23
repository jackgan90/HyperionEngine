#include "Hyperion/SceneEditing/SceneAuthoring.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
template<class TRequest, class TResult, class TFunction>
void AddAuthoring(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument, std::string InId, std::string InSummary,
                  std::string InDescription, bool bInReadOnly, TRequest InExample, TFunction InFunction)
{
	FOperationInfo Info;
	Info.Id = std::move(InId);
	Info.Summary = std::move(InSummary);
	Info.Description = std::move(InDescription);
	Info.Owner = "automation-scene";
	Info.bReadOnly = bInReadOnly;
	Info.Effects = bInReadOnly
	                   ? "Reads shared scene state."
	                   : "Updates the shared document; persistent edits use host history. No implicit disk write.";
	Info.Completion = "Main state committed; rendering observes the change on subsequent frames.";
	Info.Keywords = {"scene", "authoring", "selection", "hierarchy", "settings"};
	Info.Example = WriteRecordWire(RecordType<TRequest>(), &InExample);
	Info.Unavailable = InDocument ? "" : "This target has no scene document provider.";
	if (InDocument && InDocument->HasHistory() &&
	    (Info.Id == "scene.selection.duplicate" || Info.Id == "scene.selection.remove_keep_children"))
	{
		Info.Unavailable = "These Viewer structural actions are not supported by the Editor history contract.";
	}
	InCatalog.Register(MakeOperation<TRequest, TResult>(std::move(Info),
	                                                    [InDocument, InFunction](const TRequest& InRequest)
	                                                    {
		                                                    try
		                                                    {
			                                                    return InFunction(*InDocument, InRequest);
		                                                    }
		                                                    catch (const FSceneEditError& Error)
		                                                    {
			                                                    throw FAutomationError(Error.Code, Error.what());
		                                                    }
	                                                    }));
}
} // namespace

void RegisterSceneAuthoring(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	const std::string Document = "document-from-scene.info";
	AddAuthoring<FSceneMutationRequest, FSceneNodeInfo>(
	    InCatalog, InDocument, "scene.selection.duplicate", "Duplicate the primary selected Viewer node",
	    "Uses the Viewer resource-preserving duplication action; selects the created node. Editor has no corresponding "
	    "history-enabled action.",
	    false, {Document, 1},
	    [](auto& InDocument, const auto& InRequest)
	    {
		    InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
		    const auto Selected = InDocument.Selection().Primary();
		    if (!Selected)
		    {
			    throw std::invalid_argument("Select a primary node first");
		    }
		    const auto Copy = InDocument.CommitDuplicate(*Selected);
		    return DescribeSceneNode(InDocument, {InDocument.Id(), Copy});
	    });
	AddAuthoring<FSceneMutationRequest, FSceneDocumentInfo>(
	    InCatalog, InDocument, "scene.selection.remove_keep_children",
	    "Remove the primary Viewer node and preserve its children",
	    "Preserves child world transforms using the shared Viewer structural action. Editor does not offer this "
	    "action.",
	    false, {Document, 1},
	    [](auto& InDocument, const auto& InRequest)
	    {
		    InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
		    const auto Selected = InDocument.Selection().Primary();
		    if (!Selected)
		    {
			    throw std::invalid_argument("Select a primary node first");
		    }
		    InDocument.CommitRemoveKeepChildren(*Selected);
		    return DescribeSceneDocument(InDocument);
	    });
	AddAuthoring<FSceneMutationRequest, FSceneSelectionInfo>(
	    InCatalog, InDocument, "scene.selection.get", "Read ordered scene selection",
	    "Last handle is primary. Selection is shared with the GUI.", true, {Document, 1}, GetSceneSelection);
	AddAuthoring<FSceneSelectionRequest, FSceneSelectionInfo>(
	    InCatalog, InDocument, "scene.selection.set", "Replace ordered scene selection",
	    "Validate all handles before replacing selection. Empty clears. Does not dirty the document or create history.",
	    false, {Document, 1, {}}, SetSceneSelection);
	AddAuthoring<FSceneMetadataRequest, FSceneDocumentInfo>(
	    InCatalog, InDocument, "scene.nodes.set_metadata", "Edit node names and enabled state",
	    "1-128 distinct targets. Optional fields retain existing values when absent. One atomic history transaction.",
	    false, {Document, 1, {{{1, 0, 1}, "Renamed", true}}}, SetSceneMetadata);
	AddAuthoring<FSceneCreateRequest, FSceneNodeInfo>(
	    InCatalog, InDocument, "scene.node.create", "Create a scene object",
	    "Creates and selects a node using registered default components. Empty components creates a group. Query "
	    "types.describe for component types. Prepared model placement uses its resource provider. Supports host undo.",
	    false, {Document, 1}, CreateSceneNode);
	AddAuthoring<FSceneReparentRequest, FSceneDocumentInfo>(
	    InCatalog, InDocument, "scene.node.reparent", "Reparent a scene object",
	    "Preserves world transform by default. Rejects cycles, stale handles and non-invertible parents before "
	    "committing. Null parent makes a root.",
	    false, {Document, 1, {1, 0, 1}, {}, true}, ReparentSceneNode);
	AddAuthoring<FSceneMutationRequest, FSceneDocumentInfo>(
	    InCatalog, InDocument, "scene.selection.delete", "Delete selected subtrees",
	    "Deletes selected roots and descendants. Clears selection; Editor undo restores and reselects deleted roots "
	    "with new handles. Query selection after undo.",
	    false, {Document, 1}, DeleteSceneSelection);
	AddAuthoring<FSceneMutationRequest, FSceneSettings>(
	    InCatalog, InDocument, "scene.settings.get", "Read scene settings",
	    "Reads runtime camera, main lights and saved initial browsing view.", true, {Document, 1}, GetSceneSettings);
	AddAuthoring<FSceneSettingsRequest, FSceneDocumentInfo>(
	    InCatalog, InDocument, "scene.settings.set", "Replace scene settings",
	    "Read current settings first. Complete replacement validates referenced handles and camera values and uses "
	    "shared history.",
	    false, {Document, 1, {}}, SetSceneSettings);
}
} // namespace Hyperion
