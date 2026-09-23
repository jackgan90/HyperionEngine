#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "SceneOperations.h"

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

template<class TFunction> FOperationTask InvokeComponent(TFunction InFunction)
{
	try
	{
		return {InFunction()};
	}
	catch (const FSceneEditError& Error)
	{
		throw FAutomationError(Error.Code, Error.what());
	}
}

template<class T> void RegisterComponentBatch(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	const auto& Type = RecordType<T>();
	auto Info =
	    ComponentInfo("scene.component." + Type.Id + ".set_batch", "Edit per-object " + Type.Id, false, InDocument);
	Info.Description += " Supply parallel handles/components/values arrays to preserve different instance IDs and "
	                    "unedited values. All candidates commit together as one Undo.";
	const TSceneComponentBatchRequest<T> Example{"document-from-scene.info", 1, {{1, 0, 1}}, {Type.Id}, {T{}}};
	Info.Example = WriteRecordWire(SceneComponentBatchRequestType<T>(), &Example);
	InCatalog.Register({std::move(Info), &SceneComponentBatchRequestType<T>(), &RecordType<FSceneDocumentInfo>(), false,
	                    [InDocument](const void* InRequest)
	                    {
		                    return InvokeComponent(
		                        [&]
		                        {
			                        const auto Result = SetSceneComponentBatch(
			                            *InDocument, *static_cast<const TSceneComponentBatchRequest<T>*>(InRequest));
			                        return WriteRecordWire(RecordType<FSceneDocumentInfo>(), &Result);
		                        });
	                    }});
}

template<class T> void RegisterComponent(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	RegisterComponentBatch<T>(InCatalog, InDocument);
	const auto& Type = RecordType<T>();
	const FSceneComponentRequest Example{"document-from-scene.info", 1, {1, 0, 1}, Type.Id};
	auto Get = ComponentInfo("scene.component." + Type.Id + ".get", "Read " + Type.Id, true, InDocument);
	Get.Example = WriteRecordWire(RecordType<FSceneComponentRequest>(), &Example);
	InCatalog.Register(
	    {std::move(Get), &RecordType<FSceneComponentRequest>(), &Type, false, [InDocument](const void* InRequest)
	     {
		     return InvokeComponent(
		         [&]
		         {
			         const auto& Component =
			             GetSceneComponent(*InDocument, *static_cast<const FSceneComponentRequest*>(InRequest));
			         if (Component.Type->CppType != typeid(T))
			         {
				         throw std::invalid_argument("Component type does not match this operation");
			         }
			         return WriteRecordWire(RecordType<T>(), Component.Get());
		         });
	     }});
	auto Set = ComponentInfo("scene.component." + Type.Id + ".set", "Edit " + Type.Id, false, InDocument);
	const TSceneComponentRequest<T> SetExample{Example.Document, 1, {Example.Handle}, Type.Id, {}};
	Set.Example = WriteRecordWire(SceneComponentRequestType<T>(), &SetExample);
	InCatalog.Register({std::move(Set), &SceneComponentRequestType<T>(), &RecordType<FSceneDocumentInfo>(), false,
	                    [InDocument](const void* InRequest)
	                    {
		                    return InvokeComponent(
		                        [&]
		                        {
			                        const auto& Request = *static_cast<const TSceneComponentRequest<T>*>(InRequest);
			                        const auto Result = SetSceneComponent(
			                            *InDocument, {Request.Document, Request.Revision, Request.Handles},
			                            Request.Component, RecordType<T>(), &Request.Value);
			                        return WriteRecordWire(RecordType<FSceneDocumentInfo>(), &Result);
		                        });
	                    }});
}

void RegisterComponentDiscovery(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument)
{
	auto Info = ComponentInfo("scene.components.list", "List components on a node", true, InDocument);
	const FSceneNodeRequest Example{"document-from-scene.info", {1, 0, 1}};
	Info.Example = WriteRecordWire(RecordType<FSceneNodeRequest>(), &Example);
	InCatalog.Register({std::move(Info), &RecordType<FSceneNodeRequest>(), &RecordType<FSceneComponentList>(), false,
	                    [InDocument](const void* InRequest)
	                    {
		                    return InvokeComponent(
		                        [&]
		                        {
			                        const auto Result = ListSceneComponents(
			                            *InDocument, *static_cast<const FSceneNodeRequest*>(InRequest));
			                        return WriteRecordWire(RecordType<FSceneComponentList>(), &Result);
		                        });
	                    }});
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
	Info.Example = WriteRecordWire(RecordType<FSceneComponentStructureRequest>(), &Example);
	InCatalog.Register({std::move(Info), &RecordType<FSceneComponentStructureRequest>(),
	                    &RecordType<FSceneDocumentInfo>(), false, [InDocument](const void* InRequest)
	                    {
		                    return InvokeComponent(
		                        [&]
		                        {
			                        const auto Result = EditSceneComponentStructure(
			                            *InDocument, *static_cast<const FSceneComponentStructureRequest*>(InRequest));
			                        return WriteRecordWire(RecordType<FSceneDocumentInfo>(), &Result);
		                        });
	                    }});
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
