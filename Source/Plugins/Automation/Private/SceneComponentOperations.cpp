#include "Hyperion/AutomationHost/SceneComponentOperations.h"
#include "SceneOperationRegistration.h"

namespace Hyperion
{
namespace
{
FOperationInfo ComponentInfo(std::string InId, std::string InSummary, bool bInReadOnly, FSceneEditDocument* InDocument)
{
	return SceneComponentOperationInfo(std::move(InId), std::move(InSummary), bInReadOnly, InDocument,
	                                   "automation-scene");
}

void RegisterComponentDiscovery(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	auto Info = ComponentInfo("scene.components.list", "List components on a node", true, InDocument);
	const FSceneNodeRequest Example{"document-from-scene.info", {1, 0, 1}};
	RegisterSceneOperation<FSceneNodeRequest, FSceneComponentList>(InCatalog, std::move(Info), Example,
	                                                               [InDocument](const auto& InRequest)
	                                                               {
		                                                               return ListSceneComponents(*InDocument,
		                                                                                          InRequest);
	                                                               });
	auto Types = ComponentInfo("scene.component_types.list", "List registered scene component types", true, InDocument);
	Types.Unavailable.clear();
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FSceneComponentList>(
	    std::move(Types),
	    [](const auto&)
	    {
		    FSceneComponentList Result;
		    for (const auto& Type : SceneComponentRegistry().All())
		    {
			    Result.Components.push_back({Type->Id, Type->Id, Type->Label, Type->bRequired});
		    }
		    return Result;
	    }));
}
} // namespace

void RegisterSceneComponents(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	RegisterComponentDiscovery(InCatalog, InDocument);
	auto Info =
	    ComponentInfo("scene.components.edit_structure", "Add or remove components atomically", false, InDocument);
	const FSceneComponentStructureRequest Example{"document-from-scene.info",    1,    {{1, 0, 1}}, "camera",
	                                              RecordType<FSceneCamera>().Id, false};
	RegisterSceneOperation<FSceneComponentStructureRequest, FSceneDocumentInfo>(InCatalog, std::move(Info), Example,
	                                                                            [InDocument](const auto& InRequest)
	                                                                            {
		                                                                            return EditSceneComponentStructure(
		                                                                                *InDocument, InRequest);
	                                                                            });
	const FSceneComponentOperationOptions Options{"automation-scene", true, true};
	RegisterSceneComponentOperations<FSceneTransform>(InCatalog, InDocument, Options);
	RegisterSceneComponentOperations<FSceneModelSource>(InCatalog, InDocument, Options);
	RegisterSceneComponentOperations<FSceneModelComponent>(InCatalog, InDocument, Options);
	RegisterSceneComponentOperations<FSceneCamera>(InCatalog, InDocument, Options);
	RegisterSceneComponentOperations<FSceneDirectionalLight>(InCatalog, InDocument, Options);
	RegisterSceneComponentOperations<FSceneEnvironmentLight>(InCatalog, InDocument, Options);
	RegisterSceneComponentOperations<FScenePointLight>(InCatalog, InDocument, Options);
	RegisterSceneComponentOperations<FSceneSpotLight>(InCatalog, InDocument, Options);
}
} // namespace Hyperion
