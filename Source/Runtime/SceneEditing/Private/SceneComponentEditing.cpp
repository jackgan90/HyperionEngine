#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "Hyperion/SceneEditing/SceneComponentEditPolicy.h"

namespace Hyperion
{
const FSceneComponent& GetSceneComponent(const FSceneEditDocument& InDocument, const FSceneComponentRequest& InRequest)
{
	InDocument.RequireCurrent(InRequest.Document, InRequest.Revision);
	const auto* Component = InDocument.RequireNode(InRequest.Handle).Components.Find(InRequest.Component);
	if (!Component || !Component->Get())
	{
		throw FSceneEditError("not_found", "Node does not contain the requested component");
	}
	return *Component;
}

FSceneComponentList ListSceneComponents(const FSceneEditDocument& InDocument, const FSceneNodeRequest& InRequest)
{
	InDocument.RequireCurrent(InRequest.Document, InDocument.Target().Revision());
	const auto& Node = InDocument.RequireNode(InRequest.Handle);
	FSceneComponentList Result;
	for (const auto& Component : Node.Components.All())
	{
		if (Component.Get())
		{
			Result.Components.push_back(
			    {Component.Id, Component.Type->Id, Component.Type->Label, Component.Type->bRequired});
		}
	}
	return Result;
}

FSceneDocumentInfo EditSceneComponentStructure(FSceneEditDocument& InDocument,
                                               const FSceneComponentStructureRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (InRequest.Handles.empty() || InRequest.Handles.size() > 128)
	{
		throw std::invalid_argument("Edit between 1 and 128 distinct objects");
	}
	std::vector<FSceneNodeEdit> Edits;
	for (const auto Handle : InRequest.Handles)
	{
		auto Node = InDocument.RequireNode(Handle);
		if (InRequest.bRemove)
		{
			Node.Components.Remove(InRequest.Component);
		}
		else
		{
			AddDefaultSceneComponent(Node, InRequest.Component, InRequest.Type);
		}
		Edits.push_back({Handle, std::move(Node)});
	}
	InDocument.CommitEdits(std::move(Edits), InRequest.Revision);
	return DescribeSceneDocument(InDocument);
}

static FSceneNodeEdit PrepareCurrentSceneComponentEdit(const FSceneEditDocument& InDocument,
                                                       const FSceneComponentRequest& InRequest,
                                                       const FRecordDescriptor& InType, const void* InValue)
{
	const auto& Original = InDocument.RequireNode(InRequest.Handle);
	const auto* Found = Original.Components.Find(InRequest.Component);
	if (!Found || !Found->Get())
	{
		throw FSceneEditError("not_found", "Node does not contain the requested component");
	}
	const auto& Source = *Found;
	if (Source.Type->Record != &InType)
	{
		throw std::invalid_argument("Component type does not match the operation");
	}
	ValidateSceneComponentEdit(InType, Source.Get(), InValue);
	auto Node = Original;
	auto* Value = Node.Components.Find(InRequest.Component)->Edit();
	for (const auto& Member : InType.Members)
	{
		if (!Member.Options.bPersistent)
		{
			continue;
		}
		const auto Candidate = Member.Write(InValue);
		Member.Read(Value, Candidate, {Member.Id});
	}
	if (InType.Validate)
	{
		InType.Validate(Value);
	}
	return {InRequest.Handle, std::move(Node)};
}

FSceneDocumentInfo SetSceneComponentValues(FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest,
                                           const FRecordDescriptor& InType,
                                           std::span<const FSceneComponentEditValue> InValues)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (InValues.empty() || InValues.size() > 128)
	{
		throw std::invalid_argument("Edit between 1 and 128 distinct objects");
	}
	std::vector<FSceneNodeEdit> Edits;
	for (const auto& Value : InValues)
	{
		Edits.push_back(PrepareCurrentSceneComponentEdit(
		    InDocument, {InRequest.Document, InRequest.Revision, Value.Handle, std::string(Value.Component)}, InType,
		    Value.Value));
	}
	InDocument.CommitEdits(std::move(Edits), InRequest.Revision);
	return DescribeSceneDocument(InDocument);
}

FSceneDocumentInfo SetSceneComponent(FSceneEditDocument& InDocument, const FSceneSelectionRequest& InRequest,
                                     std::string_view InComponent, const FRecordDescriptor& InType, const void* InValue)
{
	if (InRequest.Handles.empty() || InRequest.Handles.size() > 128)
	{
		InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
		throw std::invalid_argument("Edit between 1 and 128 distinct objects");
	}
	std::vector<FSceneComponentEditValue> Values;
	for (const auto Handle : InRequest.Handles)
	{
		Values.push_back({Handle, InComponent, InValue});
	}
	return SetSceneComponentValues(InDocument, {InRequest.Document, InRequest.Revision}, InType, Values);
}

FSceneNodeEdit PrepareSceneComponentEdit(const FSceneEditDocument& InDocument, const FSceneComponentRequest& InRequest,
                                         const FRecordDescriptor& InType, const void* InValue)
{
	InDocument.RequireCurrent(InRequest.Document, InRequest.Revision);
	return PrepareCurrentSceneComponentEdit(InDocument, InRequest, InType, InValue);
}
} // namespace Hyperion
