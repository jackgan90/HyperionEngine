#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "SceneOperationRegistration.h"

namespace Hyperion
{
namespace
{
FOperationInfo ComponentInfo(std::string InId, std::string InSummary, bool bInReadOnly, FSceneEditDocument* InDocument)
{
	FOperationInfo Info;
	Info.Id = std::move(InId);
	Info.Summary = std::move(InSummary);
	Info.Description =
	    "Uses registered component reflection and the live scene document. Query scene.components.list for instance "
	    "IDs and types.describe for values. Edits require current revision and an idle host, validate the complete "
	    "candidate and commit one atomic transaction. Resource bindings remain owned by the target.";
	Info.Owner = "automation-scene";
	Info.bReadOnly = bInReadOnly;
	Info.Effects =
	    bInReadOnly ? "Reads scene component values." : "Updates shared scene/history; save explicitly to persist.";
	Info.Completion = "Main state committed; does not wait for rendering or write disk.";
	Info.Keywords = {"scene", "component", "property", "camera", "light", "model"};
	Info.Unavailable = InDocument ? "" : "This target has no scene document provider.";
	return Info;
}

template<class T> void RegisterComponentBatch(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	const auto& Type = RecordType<T>();
	auto Info =
	    ComponentInfo("scene.component." + Type.Id + ".set_batch", "Edit per-object " + Type.Id, false, InDocument);
	Info.Description += " Supply parallel handles/components/values arrays to preserve different instance IDs and "
	                    "unedited values. All candidates commit together as one Undo.";
	const TSceneComponentBatchRequest<T> Example{"document-from-scene.info", 1, {{1, 0, 1}}, {Type.Id}, {T{}}};
	RegisterSceneOperation<TSceneComponentBatchRequest<T>, FSceneDocumentInfo>(
	    InCatalog, std::move(Info), SceneComponentBatchRequestType<T>(), RecordType<FSceneDocumentInfo>(), Example,
	    [InDocument](const auto& InRequest)
	    {
		    return SetSceneComponentBatch(*InDocument, InRequest);
	    });
}

template<class T> void RegisterComponent(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	RegisterComponentBatch<T>(InCatalog, InDocument);
	const auto& Type = RecordType<T>();
	const FSceneComponentRequest Example{"document-from-scene.info", 1, {1, 0, 1}, Type.Id};
	auto Get = ComponentInfo("scene.component." + Type.Id + ".get", "Read " + Type.Id, true, InDocument);
	RegisterSceneOperation<FSceneComponentRequest, T>(
	    InCatalog, std::move(Get), Example,
	    [InDocument](const auto& InRequest)
	    {
		    const auto& Component = GetSceneComponent(*InDocument, InRequest);
		    if (Component.Type->CppType != typeid(T))
		    {
			    throw std::invalid_argument("Component type does not match this operation");
		    }
		    return *static_cast<const T*>(Component.Get());
	    });
	auto Set = ComponentInfo("scene.component." + Type.Id + ".set", "Edit " + Type.Id, false, InDocument);
	const TSceneComponentRequest<T> SetExample{Example.Document, 1, {Example.Handle}, Type.Id, {}};
	RegisterSceneOperation<TSceneComponentRequest<T>, FSceneDocumentInfo>(
	    InCatalog, std::move(Set), SceneComponentRequestType<T>(), RecordType<FSceneDocumentInfo>(), SetExample,
	    [InDocument](const auto& InRequest)
	    {
		    return SetSceneComponent(*InDocument, {InRequest.Document, InRequest.Revision, InRequest.Handles},
		                             InRequest.Component, RecordType<T>(), &InRequest.Value);
	    });
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
	RegisterComponent<FSceneTransform>(InCatalog, InDocument);
	RegisterComponent<FSceneModelSource>(InCatalog, InDocument);
	RegisterComponent<FSceneModelComponent>(InCatalog, InDocument);
	RegisterComponent<FSceneCamera>(InCatalog, InDocument);
	RegisterComponent<FSceneDirectionalLight>(InCatalog, InDocument);
	RegisterComponent<FSceneEnvironmentLight>(InCatalog, InDocument);
	RegisterComponent<FScenePointLight>(InCatalog, InDocument);
	RegisterComponent<FSceneSpotLight>(InCatalog, InDocument);
}
} // namespace Hyperion
