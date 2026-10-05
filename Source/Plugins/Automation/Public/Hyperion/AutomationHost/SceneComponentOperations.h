#pragma once
#include "Hyperion/AutomationHost/OperationRegistration.h"
#include "Hyperion/SceneEditing/SceneComponentEditing.h"

namespace Hyperion
{
struct FSceneComponentOperationOptions
{
	std::string Owner;
	bool bExposeRead = true;
	bool bExposeWrite{};
};

FOperationInfo SceneComponentOperationInfo(std::string InId, std::string InSummary, bool bInReadOnly,
                                           FSceneEditDocument* InDocument, std::string_view InOwner);
FSceneEditDocument& RequireSceneComponentDocument(FSceneEditDocument* InDocument);
void RegisterSceneComponentOperationFamily(FOperationCatalog& InCatalog, const FRecordDescriptor& InType,
                                           std::vector<FOperationDescriptor> InOperations);

namespace SceneComponentOperationDetails
{
template<class T>
FOperationDescriptor Get(FSceneEditDocument* InDocument, const FSceneComponentOperationOptions& InOptions)
{
	const auto& Type = RecordType<T>();
	const FSceneComponentRequest Example{"document-from-scene.info", 1, {1, 0, 1}, Type.Id};
	auto Info = SceneComponentOperationInfo("scene.component." + Type.Id + ".get", "Read " + Type.Id, true, InDocument,
	                                        InOptions.Owner);
	return MakeWireOperation<FSceneComponentRequest, T>(
	    std::move(Info), RecordType<FSceneComponentRequest>(), Type, Example,
	    [InDocument](const auto& InRequest)
	    {
		    const auto& Component = GetSceneComponent(RequireSceneComponentDocument(InDocument), InRequest);
		    if (Component.Type->CppType != typeid(T))
		    {
			    throw std::invalid_argument("Component type does not match this operation");
		    }
		    return *static_cast<const T*>(Component.Get());
	    });
}

template<class T>
FOperationDescriptor Set(FSceneEditDocument* InDocument, const FSceneComponentOperationOptions& InOptions,
                         const T& InExample)
{
	const auto& Type = RecordType<T>();
	const TSceneComponentRequest<T> Example{"document-from-scene.info", 1, {{1, 0, 1}}, Type.Id, InExample};
	auto Info = SceneComponentOperationInfo("scene.component." + Type.Id + ".set", "Edit " + Type.Id, false, InDocument,
	                                        InOptions.Owner);
	return MakeWireOperation<TSceneComponentRequest<T>, FSceneDocumentInfo>(
	    std::move(Info), SceneComponentRequestType<T>(), RecordType<FSceneDocumentInfo>(), Example,
	    [InDocument](const auto& InRequest)
	    {
		    return SetSceneComponent(RequireSceneComponentDocument(InDocument),
		                             {InRequest.Document, InRequest.Revision, InRequest.Handles}, InRequest.Component,
		                             RecordType<T>(), &InRequest.Value);
	    });
}

template<class T>
FOperationDescriptor SetBatch(FSceneEditDocument* InDocument, const FSceneComponentOperationOptions& InOptions,
                              const T& InExample)
{
	const auto& Type = RecordType<T>();
	const TSceneComponentBatchRequest<T> Example{"document-from-scene.info", 1, {{1, 0, 1}}, {Type.Id}, {InExample}};
	auto Info = SceneComponentOperationInfo("scene.component." + Type.Id + ".set_batch", "Edit per-object " + Type.Id,
	                                        false, InDocument, InOptions.Owner);
	Info.Description += " Supply parallel handles/components/values arrays to preserve different instance IDs and "
	                    "unedited values. All candidates commit together as one Undo.";
	return MakeWireOperation<TSceneComponentBatchRequest<T>, FSceneDocumentInfo>(
	    std::move(Info), SceneComponentBatchRequestType<T>(), RecordType<FSceneDocumentInfo>(), Example,
	    [InDocument](const auto& InRequest)
	    {
		    return SetSceneComponentBatch(RequireSceneComponentDocument(InDocument), InRequest);
	    });
}
} // namespace SceneComponentOperationDetails

// Call on Main during startup, after CPU component registration and before Seal.
// Withdraw Owner before destroying the borrowed document. Writes are an explicit opt-in.
template<class T>
void RegisterSceneComponentOperations(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument,
                                      const FSceneComponentOperationOptions& InOptions, const T& InExample = T{})
{
	InCatalog.RequireOwner();
	if (InOptions.Owner.empty() || (!InOptions.bExposeRead && !InOptions.bExposeWrite))
	{
		throw std::invalid_argument("Component operations require an owner and at least one exposure");
	}
	std::vector<FOperationDescriptor> Operations;
	if (InOptions.bExposeWrite)
	{
		Operations.push_back(SceneComponentOperationDetails::SetBatch<T>(InDocument, InOptions, InExample));
	}
	if (InOptions.bExposeRead)
	{
		Operations.push_back(SceneComponentOperationDetails::Get<T>(InDocument, InOptions));
	}
	if (InOptions.bExposeWrite)
	{
		Operations.push_back(SceneComponentOperationDetails::Set<T>(InDocument, InOptions, InExample));
	}
	RegisterSceneComponentOperationFamily(InCatalog, RecordType<T>(), std::move(Operations));
}
} // namespace Hyperion
