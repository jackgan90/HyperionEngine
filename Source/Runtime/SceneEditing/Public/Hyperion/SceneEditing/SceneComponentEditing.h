#pragma once
#include "Hyperion/SceneEditing/SceneAuthoring.h"

namespace Hyperion
{
struct FSceneComponentRequest
{
	std::string Document;
	std::uint64_t Revision{};
	FSceneHandle Handle;
	std::string Component;
};

struct FSceneComponentStructureRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Handles;
	std::string Component;
	std::string Type;
	bool bRemove{};
};

struct FSceneComponentInfo
{
	std::string Component;
	std::string Type;
	std::string Label;
	bool bRequired{};
};

struct FSceneComponentList
{
	std::vector<FSceneComponentInfo> Components;
};

template<class T> struct TSceneComponentRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Handles;
	std::string Component;
	T Value;
};

template<class T> struct TSceneComponentBatchRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Handles;
	std::vector<std::string> Components;
	std::vector<T> Values;
};

FSceneNodeEdit PrepareSceneComponentEdit(const FSceneEditDocument& InDocument, const FSceneComponentRequest& InRequest,
                                         const FRecordDescriptor& InType, const void* InValue);

template<class T>
FSceneDocumentInfo SetSceneComponentBatch(FSceneEditDocument& InDocument,
                                          const TSceneComponentBatchRequest<T>& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	const auto Count = InRequest.Handles.size();
	if (!Count || Count > 128 || InRequest.Components.size() != Count || InRequest.Values.size() != Count)
	{
		throw std::invalid_argument("Supply 1-128 distinct handles with one component ID and value per handle");
	}
	std::vector<FSceneNodeEdit> Edits;
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		Edits.push_back(PrepareSceneComponentEdit(
		    InDocument, {InRequest.Document, InRequest.Revision, InRequest.Handles[Index], InRequest.Components[Index]},
		    RecordType<T>(), &InRequest.Values[Index]));
	}
	InDocument.CommitEdits(std::move(Edits), InRequest.Revision);
	return DescribeSceneDocument(InDocument);
}

template<class T> const FRecordDescriptor& SceneComponentBatchRequestType()
{
	using FRequest = TSceneComponentBatchRequest<T>;
	static const auto Type = MakeRecord<FRequest>(
	    RecordType<T>().Id + ".batch.request",
	    {Member("document", &FRequest::Document, {.bRequired = true}),
	     Member("revision", &FRequest::Revision, {.bRequired = true}),
	     Member("handles", &FRequest::Handles,
	            {.bRequired = true, .Description = "1-128 distinct nodes in one atomic transaction."}),
	     Member("components", &FRequest::Components,
	            {.bRequired = true, .Description = "Instance ID for each corresponding handle; IDs may differ."}),
	     Member("values", &FRequest::Values,
	            {.bRequired = true,
	             .Description = "One complete value per handle. Read each target first and retain its unedited fields. "
	                            "All arrays must have equal length."})});
	return Type;
}

const FSceneComponent& GetSceneComponent(const FSceneEditDocument& InDocument, const FSceneComponentRequest& InRequest);
FSceneComponentList ListSceneComponents(const FSceneEditDocument& InDocument, const FSceneNodeRequest& InRequest);
FSceneDocumentInfo EditSceneComponentStructure(FSceneEditDocument& InDocument,
                                               const FSceneComponentStructureRequest& InRequest);
FSceneDocumentInfo SetSceneComponent(FSceneEditDocument& InDocument, const FSceneSelectionRequest& InRequest,
                                     std::string_view InComponent, const FRecordDescriptor& InType,
                                     const void* InValue);

template<class T> const FRecordDescriptor& SceneComponentRequestType()
{
	using FRequest = TSceneComponentRequest<T>;
	static const auto Type = MakeRecord<FRequest>(
	    RecordType<T>().Id + ".edit.request",
	    {Member("document", &FRequest::Document, {.bRequired = true, .Description = "Current scene document."}),
	     Member("revision", &FRequest::Revision, {.bRequired = true, .Description = "Expected scene revision."}),
	     Member("handles", &FRequest::Handles,
	            {.bRequired = true, .Description = "1-128 distinct nodes; one atomic transaction."}),
	     Member("component", &FRequest::Component,
	            {.bRequired = true, .Description = "Component instance ID from scene.components.list."}),
	     Member("value", &FRequest::Value,
	            {.bRequired = true,
	             .Description = "Complete reflected component value; read first. Runtime resource bindings are "
	                            "preserved. Immutable source fields must remain unchanged."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneComponentRequest>();
template<> const FRecordDescriptor& RecordType<FSceneComponentStructureRequest>();
template<> const FRecordDescriptor& RecordType<FSceneComponentInfo>();
template<> const FRecordDescriptor& RecordType<FSceneComponentList>();
} // namespace Hyperion
